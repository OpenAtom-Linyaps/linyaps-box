// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/io/epoll.h"
#include "linyaps_box/io/forwarder.h"
#include "linyaps_box/terminal.h"

#include <optional>

namespace linyaps_box {
class container_monitor
{
public:
    explicit container_monitor(pid_t pid)
        : pid(pid) { };
    container_monitor(const container_monitor &) = delete;
    container_monitor &operator=(const container_monitor &) = delete;
    container_monitor(container_monitor &&) = delete;
    container_monitor &operator=(container_monitor &&) = delete;
    ~container_monitor() = default;

    [[nodiscard]] auto enable_signal_forwarding() -> bool;
    auto enable_io_forwarding(terminal_master master,
                              const linyaps_box::utils::file_descriptor &in,
                              const linyaps_box::utils::file_descriptor &out) -> void;
    auto wait_container_exit() -> int;

private:
    auto handle_signals() -> void;
    bool child_exited{ false };
    pid_t pid;
    int exit_code{ 0 };
    utils::file_descriptor signal_fd;
    std::optional<terminal_master> master;
    std::optional<utils::file_descriptor> master_out;
    io::Epoll epoll;
    std::optional<io::Forwarder> in_fwd;
    std::optional<io::Forwarder> out_fwd;
    std::optional<terminal_slave> host_tty;
};
} // namespace linyaps_box
