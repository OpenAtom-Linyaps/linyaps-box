// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/infra/process_handle.h"

#include "linyaps_box/io/stream.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/process.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <array>
#include <charconv>
#include <system_error>

namespace linyaps_box::infra {

namespace {

using os::make_error_code;

bool pidfd_supported{ true };

auto to_process_state(char c) noexcept -> process_state
{
    switch (c) {
    case 'R':
        return process_state::running;
    case 'S':
        return process_state::sleeping;
    case 'D':
        return process_state::disk_sleep;
    case 'T':
        return process_state::stopped;
    case 't':
        return process_state::tracing_stop;
    case 'X':
        return process_state::dead;
    case 'Z':
        return process_state::zombie;
    case 'P':
        return process_state::parked;
    case 'I':
        return process_state::idle;
    default:
        return process_state::unknown;
    }
}

constexpr auto stat_buf_size = std::size_t{ 1024 };

auto read_stat_through(utils::file_descriptor_ref proc_fd) noexcept -> os::Result<process_stat>
{
    using os::sys::access_mode;
    using os::sys::open_flag;
    using os::sys::open_option;

    auto stat_fd_ref =
      os::openat(proc_fd, "stat", open_option{ open_flag::cloexec, access_mode::read_only });
    if (UNLIKELY(!stat_fd_ref)) {
        return os::unexpected{ stat_fd_ref.error() };
    }
    auto stat_fd = std::move(*stat_fd_ref);

    utils::uninit_vector<std::byte> buf;
    buf.reserve(stat_buf_size);
    auto bytes_read_res = io::read_to_end(stat_fd, buf);
    if (UNLIKELY(!bytes_read_res)) {
        return os::unexpected{ bytes_read_res.error() };
    }

    return parse_proc_stat(
      std::string_view{ reinterpret_cast<char *>(buf.data()), *bytes_read_res }); // NOLINT
}

} // namespace

auto parse_proc_stat(std::string_view line) noexcept -> os::Result<process_stat>
{
    const auto last_paren = line.rfind(')');
    if (UNLIKELY(last_paren == std::string_view::npos)) {
        return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto sub = line.substr(last_paren + 1);

    // Field 3: state — first non-space character after the closing paren.
    std::size_t pos{ 0 };
    while (pos < sub.size() && sub[pos] == ' ') {
        ++pos;
    }

    if (UNLIKELY(pos >= sub.size())) {
        return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }
    const auto state = to_process_state(sub[pos]);

    // Field 22: starttime — skip 19 whitespace-separated tokens after state.
    std::size_t token_index{ 0 };
    std::size_t token_start{ std::string_view::npos };
    bool in_token{ false };

    for (std::size_t i = pos + 1; i < sub.size(); ++i) {
        const auto ch = sub[i];
        if (ch == ' ' || ch == '\n' || ch == '\t') {
            if (!in_token) {
                continue;
            }

            in_token = false;
            if (token_index != 19) {
                continue;
            }

            std::uint64_t start_time{ 0 };
            const auto [ptr, ec] =
              std::from_chars(sub.data() + token_start, sub.data() + i, start_time);
            if (UNLIKELY(ec != std::errc{ })) {
                return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
            }

            return process_stat{ state, start_time };
        }

        if (!in_token) {
            in_token = true;
            ++token_index;

            if (token_index == 19) {
                token_start = i;
            }
        }
    }

    if (in_token && token_index == 19) {
        std::uint64_t start_time{ 0 };
        const auto [ptr, ec] =
          std::from_chars(sub.data() + token_start, sub.data() + sub.size(), start_time);
        if (ec != std::errc{ }) {
            return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
        }

        return process_stat{ state, start_time };
    }

    return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
}

auto process_handle::open(pid_t pid) -> os::Result<process_handle>
{
    using os::sys::access_mode;
    using os::sys::open_flag;
    using os::sys::open_option;

    if (UNLIKELY(pid < 0)) {
        return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    std::array<char, std::numeric_limits<pid_t>::digits10 + 2> buf; // NOLINT
    auto [ptr, err] = std::to_chars(buf.begin(), buf.end(), pid);
    if (UNLIKELY(err != std::errc{ })) {
        return os::unexpected{ std::make_error_code(err) };
    }
    *ptr = 0;

    auto proc_fd = os::open(std::filesystem::path{ "/proc" } / std::string_view{ buf.data() },
                            open_option{ open_flag::cloexec, access_mode::path })
                     .transform_error([](const std::error_code &ec) {
                         return (ec == std::errc::no_such_file_or_directory)
                           ? std::make_error_code(std::errc::no_such_process)
                           : ec;
                     });

    if (UNLIKELY(!proc_fd)) {
        return os::unexpected{ proc_fd.error() };
    }

    std::optional<utils::file_descriptor> pidfd;
    if (pidfd_supported) {
        auto ret = os::pidfd_open(pid);
        if (ret.has_value()) {
            pidfd = std::move(*ret);
        } else if (ret.error() == std::errc::function_not_supported) {
            pidfd_supported = false;
        }
    }

    return process_handle{ pid, std::move(*proc_fd), std::move(pidfd) };
}

auto process_handle::status() const noexcept -> os::Result<process_stat>
{
    return read_stat_through(proc_fd_.ref());
}

auto process_handle::send_signal(int sig) noexcept -> os::Result<void>
{
    if (pidfd_.has_value()) {
        auto sent = os::pidfd_send_signal(pidfd_->ref(), sig);
        if (UNLIKELY(!sent)) {
            if (sent.error() == std::errc::no_such_process) {
                return { };
            }

            // No kill fallback on pidfd_send_signal failure.
            // ENOSYS is impossible in normal operation post-pidfd_open,
            // any error suggests a forced downgrade attack or dead process.
            // Falling back risks  signaling a recycled PID via TOCTOU.
            return os::unexpected{ sent.error() };
        }

        return { };
    }

    // re-check liveness through the pinned fd to narrow the TOCTOU window.
    // The O_PATH fd prevents PID recycling, so the only residual race is the process exiting
    // between this check and the signal
    auto st = read_stat_through(proc_fd_.ref());
    if (UNLIKELY(!st)) {
        return os::unexpected{ std::move(st).error() };
    }

    if (UNLIKELY(st->state == process_state::zombie || st->state == process_state::dead)) {
        return os::unexpected{ make_error_code(ESRCH) };
    }

    auto ret = os::kill_process(pid_, sig);
    if (LIKELY(ret.has_value())) {
        return { };
    }

    const auto &err = ret.error();
    if (err == std::errc::no_such_process) {
        return { };
    }

    return os::unexpected{ err };
}

} // namespace linyaps_box::infra
