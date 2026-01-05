// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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

#include <cassert>

namespace linyaps_box {

auto container_monitor::enable_signal_forwarding() -> bool
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

        if (ret.status == linyaps_box::utils::WaitStatus::NoChild) {
            // no child process to wait, just exit
            return false;
        }

        break;
    }

    auto signalfd_pollable = epoll.add(signal_fd, EPOLLIN);
    assert(signalfd_pollable);
    if (!UNLIKELY(signalfd_pollable)) {
        throw std::runtime_error("failed to add signalfd to epoll");
    }

    return true;
}

auto container_monitor::handle_signals() -> void
{
    while (true) {
        struct signalfd_siginfo info{};
        auto status = signal_fd.read(info);

        if (status == linyaps_box::utils::file_descriptor::IOStatus::TryAgain) {
            break;
        }

        if (status != linyaps_box::utils::file_descriptor::IOStatus::Success) {
            throw std::runtime_error("failed to read signalfd");
        }

        switch (info.ssi_signo) {
        case SIGCHLD: {
            auto res = linyaps_box::utils::waitpid(pid, WNOHANG);
            if (res.status == linyaps_box::utils::WaitStatus::Reaped) {
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
                ::kill(pid, info.ssi_signo);
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

    static constexpr auto buffer_size = 256 * 1024;

    if (!child_exited) {
        in_fwd.emplace(epoll, buffer_size);
        in_fwd->set_src(in);
        in_fwd->set_dst(this->master->get());
    }

    out_fwd.emplace(epoll, buffer_size);
    out_fwd->set_src(master_out.value());
    out_fwd->set_dst(out);
}

auto container_monitor::wait_container_exit() -> int
{
    bool force_poll{ true };

    while (!child_exited || out_fwd) {
        const auto timeout = (force_poll || child_exited) ? 0 : -1;
        const auto &events = epoll.wait(timeout);

        force_poll = false;
        bool in_active{ false };
        bool out_active{ false };

        for (const auto &ev : events) {
            const auto fd = ev.data.fd;

            if (fd == signal_fd.get()) {
                handle_signals();

                if (child_exited) {
                    in_fwd.reset();
                }

                continue;
            }

            // EPOLLIN/EPOLLOUT/EPOLLERR/EPOLLHUP events will be handled by drive_fwd
            if (in_fwd
                && (ev.data.fd == in_fwd->src().get() || ev.data.fd == in_fwd->dst().get())) {
                in_active = true;
            }

            if (out_fwd
                && (ev.data.fd == out_fwd->src().get() || ev.data.fd == out_fwd->dst().get())) {
                out_active = true;
            }
        }

        auto drive_fwd = [&](std::optional<io::Forwarder> &fwd, bool triggered) {
            if (!fwd) {
                return;
            }

            if (triggered || timeout == 0 || child_exited) {
                auto pull_status = fwd->pull();
                auto push_status = fwd->push();

                if (pull_status == io::Forwarder::Status::Finished
                    || push_status == io::Forwarder::Status::Finished) {
                    fwd.reset();
                } else if (pull_status == io::Forwarder::Status::Continue
                           || push_status == io::Forwarder::Status::Continue) {
                    force_poll = true;
                }
            }
        };

        drive_fwd(in_fwd, in_active);
        drive_fwd(out_fwd, out_active);
    }

    return exit_code;
}

} // namespace linyaps_box
