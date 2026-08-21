// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <zeus/expected.hpp>

#include <cstdint>
#include <string_view>

#include <sys/types.h>

namespace linyaps_box::utils {

enum class parse_stat_error : uint8_t {
    open_failed,
    read_failed,
    invalid_format,
    parse_number_failed
};

constexpr auto format_as(parse_stat_error e) -> std::string_view
{
    switch (e) {
    case parse_stat_error::open_failed:
        return "failed to open stat file";
    case parse_stat_error::read_failed:
        return "failed to read content from stat file";
    case parse_stat_error::invalid_format:
        return "stat file format is invalid";
    case parse_stat_error::parse_number_failed:
        return "failed to parse start time to a number";
    }

    __builtin_unreachable();
}

auto read_process_start_time(pid_t pid) noexcept -> zeus::expected<uint64_t, parse_stat_error>;

} // namespace linyaps_box::utils
