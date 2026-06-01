// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_monitor.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/process.h"
#include "linyaps_box/utils/signal.h"
#include "linyaps_box/utils/terminal.h"
#include "linyaps_box/utils/utils.h"

#include <sys/signalfd.h>

#include <algorithm>

namespace linyaps_box {

auto container_monitor::enable_signal_forwarding() -> void
{
    sigset_t set;
    utils::sigfillset(set);
    utils::sigprocmask(SIG_BLOCK, set, nullptr);

    signal_fd = utils::create_signalfd(set);

    // try to reap child process. Child process may exit before we create signalfd and it wouldn't
    // receive SIGCHLD anymore. If we don't do this, the child process (or its children) will be
    // zombie process

    while (true) {
        auto ret = linyaps_box::utils::waitpid(-1, WNOHANG);
        if (ret.status == linyaps_box::utils::WaitStatus::Reaped) {
            if (ret.pid == pid) {
                // maybe child exited and output something, we should't exit here immediately
                LINYAPS_BOX_DEBUG() << "child exited early with code " << ret.exit_code;
                child_exited = true;
                exit_code = WIFSIGNALED(ret.exit_code) ? 128 + WTERMSIG(ret.exit_code)
                                                       : WEXITSTATUS(ret.exit_code);
            }

            continue;
        }

        break;
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
    if (utils::isatty(in)) {
        host_tty.emplace(in.duplicate());
    } else if (utils::isatty(out)) {
        host_tty.emplace(out.duplicate());
    } else {
        auto default_tty = utils::open("/dev/tty", O_RDWR | O_CLOEXEC);
        host_tty.emplace(std::move(default_tty));
    }

    host_tty->set_raw();

    this->master = std::move(master);

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

    if (in_fwd) {
        in_fwd->drive();
    }

    if (out_fwd) {
        out_fwd->drive();
    }

    if (in_fwd && in_fwd->is_finished()) {
        in_fwd.reset();
    }

    if (out_fwd && out_fwd->is_finished()) {
        out_fwd.reset();
    }
}

auto container_monitor::wait_container_exit() -> int
{
    // After we enable io forwarding, there may be some data in
    // the buffers, we should try to forward them immediately before waiting for epoll events.
    bool need_immediate_spin{ true };
    while (!child_exited || out_fwd) {
        auto timeout = need_immediate_spin ? 0 : -1;
        const auto events = epoll.wait(timeout);

        // handle signals at first to avoid unnecessary latency
        const auto *triggered_signal = std::find_if(events.cbegin(),
                                                    events.cend(),
                                                    [signal_fd = signal_fd.get()](const auto &e) {
                                                        return e.data.fd == signal_fd;
                                                    });

        if (triggered_signal != events.cend()) {
            handle_signals();

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

            if ((ev.events & (EPOLLERR | EPOLLHUP)) != 0) {
                if (in_fwd) {
                    if (ev.data.fd == in_fwd->src().get()) {
                        in_fwd->mark_src_eof();
                    }

                    if (ev.data.fd == in_fwd->dst().get()) {
                        in_fwd->mark_dst_failed();
                    }
                }

                if (out_fwd) {
                    if (ev.data.fd == out_fwd->src().get()) {
                        out_fwd->mark_src_eof();
                    }

                    if (ev.data.fd == out_fwd->dst().get()) {
                        out_fwd->mark_dst_failed();
                    }
                }
            }
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
