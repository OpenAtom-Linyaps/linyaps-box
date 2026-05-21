// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/container_monitor.h"
#include "linyaps_box/socket.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/process.h"
#include "linyaps_box/utils/session.h"
#include "nlohmann/json.hpp"

#include <csignal> // IWYU pragma: keep
#include <utility>

#include <unistd.h>

linyaps_box::container_ref::container_ref(status_directory status_dir, std::string id)
    : id_(std::move(id))
    , status_dir_(std::move(status_dir))
{
}

linyaps_box::container_ref::~container_ref() noexcept = default;

linyaps_box::container_status_t linyaps_box::container_ref::status() const
{
    return status_dir_.read();
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

    std::ignore = utils::prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);

    std::optional<unix_socket> recv_socketpair;
    if (option.proc.terminal && !option.console_socket) {
        auto [socket1, socket2] = linyaps_box::unix_socket::pair();
        option.console_socket = unix_socket{ std::move(socket1) };
        recv_socketpair = unix_socket{ std::move(socket2) };
    }

    auto child = fork();
    if (child < 0) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (child == 0) {
        if (option.console_socket) {
            utils::setsid();
            auto [master, slave] = create_pty_pair();

            slave.setup_stdio();
            slave.set_size({ });

            option.console_socket->send_fd(std::move(master).take());
            option.console_socket.reset();
        }

        std::vector<const char *> argv{ "nsenter", "--target", target.c_str() };

        auto config_str = status_dir_.read_config();
        auto config = nlohmann::json::parse(config_str);
        const auto &linux = config.at("linux");
        if (linux.is_null()) {
            throw std::runtime_error("container config missing linux section");
        }

        // FIXME: we assume that container always unshare some namespaces for now
        // support exec commands without setns in the future
        for (const auto &ns : linux.at("namespaces")) {
            const auto &type = ns.at("type");
            if (type == "pid") {
                argv.push_back("--pid");
            } else if (type == "mount") {
                argv.push_back("--mount");
            } else if (type == "uts") {
                argv.push_back("--uts");
            } else if (type == "ipc") {
                argv.push_back("--ipc");
            } else if (type == "network") {
                argv.push_back("--net");
            } else if (type == "user") {
                argv.push_back("--user");
            } else if (type == "cgroup") {
                argv.push_back("--cgroup");
            }
        }
        argv.push_back("--preserve-credentials");

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

        ::execvpe("nsenter", const_cast<char **>(argv.data()), const_cast<char **>(c_env.data()));
        _exit(EXIT_FAILURE);
    }

    container_monitor monitor{ child };
    monitor.enable_signal_forwarding();

    auto in = utils::file_descriptor{ STDIN_FILENO, false };
    auto out = utils::file_descriptor{ STDOUT_FILENO, false };

    bool changed{ false };
    auto in_flags = in.flags();
    auto out_flags = out.flags();

    auto restore_if_changed = utils::make_defer([&]() noexcept {
        if (!changed) {
            return;
        }

        try {
            in.set_flags(in_flags);
            out.set_flags(out_flags);
        } catch (const std::exception &e) {
            LINYAPS_BOX_ERR()
              << "failed to restore stdin/stdout flags, some behavior may be unexpected: "
              << e.what();
        }
    });

    [&recv_socketpair, &monitor, &in, &out, &changed]() {
        if (!recv_socketpair) {
            return;
        }

        LINYAPS_BOX_DEBUG() << "Container requires a terminal";

        std::string payload;
        auto master = terminal_master{ recv_socketpair->recv_fd(payload) };

        recv_socketpair->release();

        in.set_nonblock(true);
        out.set_nonblock(true);
        changed = true;

        monitor.enable_io_forwarding(std::move(master), in, out);
    }();

    return monitor.wait_container_exit();
}

const linyaps_box::status_directory &linyaps_box::container_ref::status_dir() const
{
    return status_dir_;
}

const std::string &linyaps_box::container_ref::get_id() const
{
    return id_;
}
