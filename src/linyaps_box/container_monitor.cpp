// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_monitor.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/process.h"
#include "linyaps_box/utils/signal.h"
#include "linyaps_box/utils/terminal.h"
#include "linyaps_box/utils/utils.h"

#include <sys/signalfd.h>

#include <algorithm>

namespace linyaps_box {
namespace {

// Detect a local terminal to mirror window-resize events from.
// Prefers stdin, then stdout; falls back to /dev/tty, then /dev/console.
auto detect_host_tty(const linyaps_box::utils::file_descriptor &in,
                     const linyaps_box::utils::file_descriptor &out)
  -> std::optional<terminal_slave>
{
    if (utils::isatty(in)) {
        return terminal_slave{ in.duplicate() };
    }

    if (utils::isatty(out)) {
        return terminal_slave{ out.duplicate() };
    }

    // No controlling terminal — try /dev/tty, then /dev/console as last resort.
    for (const auto *path : { "/dev/tty", "/dev/console" }) {
        try {
            return terminal_slave{ utils::open(path, O_RDWR | O_CLOEXEC) };
        } catch (const std::system_error &) {
            continue;
        }
    }

    return std::nullopt;
}

// Dispatch EPOLLERR / EPOLLHUP on a forwarder's src/dst fds.
void handle_fd_error(const struct epoll_event &ev,
                     std::optional<io::Forwarder> &in_fwd,
                     std::optional<io::Forwarder> &out_fwd)
{
    if ((ev.events & (EPOLLERR | EPOLLHUP)) == 0) {
        return;
    }

    auto mark = [fd = ev.data.fd](io::Forwarder &fwd) {
        if (fwd.src().get() == fd) {
            fwd.mark_src_eof();
        }
        if (fwd.dst().get() == fd) {
            fwd.mark_dst_failed();
        }
    };

    if (in_fwd) {
        mark(*in_fwd);
    }
    if (out_fwd) {
        mark(*out_fwd);
    }
}

} // anonymous namespace

auto container_monitor::enable_signal_forwarding() -> void
{
    sigset_t set;
    utils::sigfillset(set);
    utils::sigprocmask(SIG_BLOCK, set, nullptr);

    signal_fd = utils::create_signalfd(set);

    // Reap any children that exited before signalfd was installed.
    // Without this they'd become zombies — signalfd only delivers SIGCHLD
    // for future events.
    while (true) {
        auto ret = linyaps_box::utils::waitpid(-1, WNOHANG);
        if (ret.status != linyaps_box::utils::WaitStatus::Reaped) {
            break;
        }

        if (ret.pid == pid) {
            child_exited = true;
            exit_code = WIFSIGNALED(ret.exit_code) ? 128 + WTERMSIG(ret.exit_code)
                                                   : WEXITSTATUS(ret.exit_code);
        }
    }

    auto signalfd_pollable = epoll.add(signal_fd, EPOLLIN);
    if (!UNLIKELY(signalfd_pollable)) {
        throw std::runtime_error("failed to add signalfd to epoll");
    }
}

auto container_monitor::handle_signals() -> void
{
    while (true) {
        struct signalfd_siginfo info{ };
        auto [status, bytes_read] = signal_fd.read(info);

        if (status == utils::IOStatus::TryAgain) {
            break;
        }

        if (status != utils::IOStatus::Success) {
            throw std::runtime_error("failed to read signalfd");
        }

        switch (info.ssi_signo) {
        case SIGCHLD: {
            auto res = linyaps_box::utils::waitpid(-1, WNOHANG);
            if (res.status != linyaps_box::utils::WaitStatus::Reaped) {
                break;
            }

            if (res.pid == pid) {
                child_exited = true;
                exit_code = WIFSIGNALED(res.exit_code) ? 128 + WTERMSIG(res.exit_code)
                                                       : WEXITSTATUS(res.exit_code);
            }
        } break;

        case SIGWINCH: {
            if (master && host_tty) {
                master->resize(host_tty->get_size());
            }
        } break;

        default: {
            if (!child_exited) {
                ::kill(pid, static_cast<int>(info.ssi_signo));
            }
        } break;
        }
    }
}

auto container_monitor::enable_io_forwarding(terminal_master master,
                                             const linyaps_box::utils::file_descriptor &in,
                                             const linyaps_box::utils::file_descriptor &out) -> void
{
    host_tty = detect_host_tty(in, out);
    if (host_tty) {
        host_tty->set_raw();
    }

    this->master = std::move(master);

    // The PTY master fd is used bidirectionally: we write stdin into it AND
    // read its output back to stdout.  epoll needs two independent fd
    // registrations, so we duplicate the master fd here.
    this->master->get().set_nonblock(true);
    master_out = this->master.value().get().duplicate();
    master_out->set_nonblock(true);

    // Linux TTY buffer is hardcoded to 4K (N_TTY_BUF_SIZE). Using an 8K buffer
    // allows draining it in one shot and avoiding a redundant read() returning EAGAIN.
    constexpr auto buffer_size{ 8 * 1024 };
    if (!child_exited) {
        in_fwd.emplace(epoll, buffer_size);
        in_fwd->set_src(in);
        in_fwd->set_dst(this->master->get());
    }

    out_fwd.emplace(epoll, buffer_size);
    out_fwd->set_src(master_out.value());
    out_fwd->set_dst(out);

    // Prime the IO loop — drain any data already buffered.
    auto drive_and_cleanup = [](std::optional<io::Forwarder> &fwd) {
        if (!fwd) {
            return;
        }
        fwd->drive();
        if (fwd->is_finished()) {
            fwd.reset();
        }
    };

    drive_and_cleanup(in_fwd);
    drive_and_cleanup(out_fwd);
}

auto container_monitor::wait_container_exit() -> int
{
    // After IO forwarding is set up, there may already be data in flight.
    // Spin once with timeout=0 to drain it without blocking.
    bool need_immediate_spin{ true };

    while (!child_exited || out_fwd) {
        const auto timeout = need_immediate_spin ? 0 : -1;
        const auto events = epoll.wait(timeout);

        // Handle signals before data forwarding to keep latency low.
        const auto *triggered_signal = std::find_if(events.cbegin(),
                                                    events.cend(),
                                                    [signal_fd = signal_fd.get()](const auto &e) {
                                                        return e.data.fd == signal_fd;
                                                    });

        if (triggered_signal != events.cend()) {
            handle_signals();

            // Once the child has exited, the PTY will shut down soon.
            // Mark the forwarders so the event loop can drain remaining
            // output and then terminate.
            if (child_exited) {
                if (in_fwd) {
                    in_fwd->mark_dst_failed();
                }
                if (out_fwd) {
                    out_fwd->mark_src_eof();
                }
            }
        }

        for (const auto &ev : events) {
            if (ev.data.fd == signal_fd.get()) {
                continue;
            }
            handle_fd_error(ev, in_fwd, out_fwd);
        }

        bool in_work{ false };
        bool out_work{ false };

        if (in_fwd) {
            in_work = in_fwd->drive();
        }
        if (out_fwd) {
            out_work = out_fwd->drive();
        }

        need_immediate_spin = in_work || out_work;

        // Release finished forwarders so the loop exit condition can
        // eventually be satisfied.
        if (in_fwd && in_fwd->is_finished()) {
            in_fwd.reset();
        }
        if (out_fwd && out_fwd->is_finished()) {
            out_fwd.reset();
        }
    }

    return exit_code;
}

} // namespace linyaps_box
