// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/container_monitor.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/process.h"
#include "linyaps_box/utils/session.h"
#include "linyaps_box/utils/socket.h"
#include "linyaps_box/utils/terminal.h"

#include <csignal> // IWYU pragma: keep
#include <utility>

#include <unistd.h>

linyaps_box::container_ref::container_ref(const status_directory &status_dir, std::string id)
    : id_(std::move(id))
    , status_dir_(status_dir)
{
}

linyaps_box::container_ref::~container_ref() noexcept = default;

linyaps_box::container_status_t linyaps_box::container_ref::status() const
{
    return this->status_dir_.read(this->id_);
}

void linyaps_box::container_ref::kill(int signal) const
{
    auto pid = this->status().PID;

    LINYAPS_BOX_DEBUG() << "kill process " << pid << " with signal " << signal;
    if (::kill(pid, signal) == 0) {
        return;
    }

    std::stringstream ss;
    ss << "failed to kill process " << pid << " with signal " << signal;
    throw std::system_error(errno, std::system_category(), std::move(ss).str());
}

auto linyaps_box::container_ref::exec(exec_container_option option) -> int
{
    auto target = std::to_string(this->status().PID);

    // TODO: support detach later
    utils::prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);

    std::optional<unixSocketClient> recv_socketpair;
    if (option.proc.terminal && !option.console_socket) {
        auto [socket1, socket2] = utils::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        option.console_socket = unixSocketClient{ std::move(socket1) };
        recv_socketpair = unixSocketClient{ std::move(socket2) };
    }

    auto child = fork();
    if (child < 0) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (child == 0) {
        // TODO: create terminal after rewrite exec. it should be created in the container namespace
        if (option.console_socket) {
            utils::setsid();
            auto [master, slave] = create_pty_pair();

            slave.setup_stdio();
            // TODO: use fchown after we implement exec option `--user`
            slave.set_size({});

            option.console_socket->send_fd(std::move(master).take());
            option.console_socket.reset();
        }

        std::vector<const char *> argv{
            "nsenter",
            "--target",
            target.c_str(),
            "--user",
            "--mount",
            "--pid",
            // FIXME:
            // Old nsenter command do not support --wdns,
            // so we have to implement nsenter by ourself in the future.
            "--preserve-credentials",
        };

        for (const auto &arg : option.proc.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        std::vector<const char *> c_env;
        c_env.reserve(option.proc.env.size());
        for (const auto &env : option.proc.env) {
            c_env.push_back(env.c_str());
        }
        c_env.push_back(nullptr);

        // FIXME:
        // We only handle the command arguments for now
        // here are some other fields in process we need to consider:
        // terminal
        // console.height
        // console.width
        // cwd
        // env
        // rlimits
        // apparmor_profile
        // capabilities
        // no_new_privileges
        // oom_score_adj

        ::execvpe("nsenter", const_cast<char **>(argv.data()), const_cast<char **>(c_env.data()));
        _exit(EXIT_FAILURE);
    }

    auto in = utils::file_descriptor{ utils::fileno(stdin), false };
    auto out = utils::file_descriptor{ utils::fileno(stdout), false };

    container_monitor monitor{ child };

    [&recv_socketpair, &monitor, &in, &out]() {
        if (!recv_socketpair) {
            return;
        }

        LINYAPS_BOX_DEBUG() << "Container requires a terminal";

        std::string payload;
        auto master = terminal_master{ recv_socketpair->recv_fd(payload) };

        recv_socketpair->release();

        in.set_nonblock(true);
        out.set_nonblock(true);

        monitor.enable_io_forwarding(std::move(master), in, out);
    }();

    monitor.enable_signal_forwarding();
    return monitor.wait_container_exit();
}

const linyaps_box::status_directory &linyaps_box::container_ref::status_dir() const
{
    return this->status_dir_;
}

const std::string &linyaps_box::container_ref::get_id() const
{
    return this->id_;
}
