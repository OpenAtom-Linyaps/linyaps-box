// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/process_stat.h"

#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/io.h"

#include <fmt/format.h>

#include <charconv>

namespace linyaps_box::utils {

auto read_process_start_time(pid_t pid) noexcept -> zeus::expected<uint64_t, parse_stat_error>
{
    constexpr auto max_digits10 = std::numeric_limits<pid_t>::digits10 + 1;
    std::array<char, 12 + max_digits10> buf{ };
    fmt::format_to_n(buf.data(), buf.size(), "/proc/{}/stat", pid);
    const std::filesystem::path stat{ buf.data() };

    auto fd_ret = os::open(
      stat,
      os::sys::open_option{ os::sys::open_flag::cloexec, os::sys::access_mode::read_only });
    if (!fd_ret) {
        const auto &err = fd_ret.error();
        if (err == std::errc::no_such_file_or_directory) {
            return 0;
        }

        return zeus::unexpected{ parse_stat_error::open_failed };
    }
    auto fd = std::move(*fd_ret);

    std::array<char, 1024> content{ };
    auto ret = os::read(fd, as_writable_bytes(span{ content }));
    if (!ret) {
        return zeus::unexpected{ parse_stat_error::read_failed };
    }

    const std::string_view line{ content.begin(), *ret };
    auto last_paren = line.rfind(')');
    if (last_paren == std::string_view::npos) {
        return zeus::unexpected{ parse_stat_error::invalid_format };
    }

    auto sub_line = line.substr(last_paren + 1);

    std::size_t token_index{ 0 };
    std::size_t token_start{ std::string_view::npos };
    bool in_token{ false };

    for (std::size_t i = 0; i < sub_line.size(); ++i) {
        const auto ch = sub_line[i];
        if (ch == ' ' || ch == '\n' || ch == '\t') {
            if (!in_token) {
                continue;
            }

            in_token = false;
            if (token_index != 20) {
                continue;
            }

            uint64_t start_time{ 0 };
            auto [ptr, ec] =
              std::from_chars(sub_line.cbegin() + token_start, sub_line.cbegin() + i, start_time);
            if (ec == std::errc{ }) {
                return start_time;
            }

            return zeus::unexpected{ parse_stat_error::parse_number_failed };
        }

        if (!in_token) {
            in_token = true;
            ++token_index;
            if (token_index == 20) {
                token_start = i;
            }
        }
    }

    if (in_token && token_index == 20) {
        uint64_t start_time{ 0 };
        auto [ptr, ec] = std::from_chars(sub_line.cbegin() + token_start,
                                         sub_line.cbegin() + sub_line.size(),
                                         start_time);
        if (ec == std::errc{ }) {
            return start_time;
        }

        return zeus::unexpected{ parse_stat_error::parse_number_failed };
    }

    return zeus::unexpected{ parse_stat_error::invalid_format };
}

} // namespace linyaps_box::utils
