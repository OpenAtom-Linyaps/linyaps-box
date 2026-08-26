// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <optional>
#include <string_view>

namespace linyaps_box::infra {

enum class process_state : std::uint8_t {
    running,      // R
    sleeping,     // S
    disk_sleep,   // D
    stopped,      // T
    tracing_stop, // t
    dead,         // X
    zombie,       // Z
    parked,       // P
    idle,         // I
    unknown,
};

constexpr auto format_as(process_state s) noexcept -> std::string_view
{
    using namespace std::string_view_literals;
    switch (s) {
    case process_state::running:
        return "running"sv;
    case process_state::sleeping:
        return "sleeping"sv;
    case process_state::disk_sleep:
        return "disk_sleep"sv;
    case process_state::stopped:
        return "stopped"sv;
    case process_state::tracing_stop:
        return "tracing_stop"sv;
    case process_state::dead:
        return "dead"sv;
    case process_state::zombie:
        return "zombie"sv;
    case process_state::parked:
        return "parked"sv;
    case process_state::idle:
        return "idle"sv;
    case process_state::unknown:
        return "unknown"sv;
    }

    __builtin_unreachable();
}

struct process_stat
{
    process_state state{ process_state::unknown };
    std::uint64_t start_time{ 0 };

    friend constexpr auto operator==(const process_stat &a, const process_stat &b) noexcept -> bool
    {
        return a.state == b.state && a.start_time == b.start_time;
    }

    friend constexpr auto operator!=(const process_stat &a, const process_stat &b) noexcept -> bool
    {
        return !(a == b);
    }
};

// Parses one line of /proc/<pid>/stat (format documented in proc(5), fields 3 and 22).
[[nodiscard]] auto parse_proc_stat(std::string_view line) noexcept -> os::Result<process_stat>;

class process_handle
{
public:
    [[nodiscard]] static auto open(pid_t pid) -> os::Result<process_handle>;

    [[nodiscard]] auto status() const noexcept -> os::Result<process_stat>;

    auto send_signal(int sig) noexcept -> os::Result<void>;

    [[nodiscard]] auto pid() const noexcept -> pid_t { return pid_; }

    [[nodiscard]] auto fd() const noexcept -> utils::file_descriptor_ref { return proc_fd_.ref(); }

    process_handle(const process_handle &) = delete;
    auto operator=(const process_handle &) -> process_handle & = delete;

    process_handle(process_handle &&) noexcept = default;
    auto operator=(process_handle &&) noexcept -> process_handle & = default;

    ~process_handle() noexcept = default;

private:
    process_handle(pid_t pid,
                   utils::file_descriptor proc_fd,
                   std::optional<utils::file_descriptor> pidfd) noexcept
        : pidfd_(std::move(pidfd))
        , proc_fd_(std::move(proc_fd))
        , pid_(pid)
    {
    }

    std::optional<utils::file_descriptor> pidfd_;
    utils::file_descriptor proc_fd_;
    pid_t pid_;
};

} // namespace linyaps_box::infra
