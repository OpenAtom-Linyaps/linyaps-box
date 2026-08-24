// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include <sys/types.h>

namespace linyaps_box::log {

// NOTE: Values are ordered from most severe to least.
// `static_cast<level>(uint8_t)` from untrusted wire data MUST be validated
// against `debug` before use (see message.cpp deserialize).
enum class level : std::uint8_t {
    fatal,
    error,
    warn,
    info,
    debug,
};

enum class output_format : std::uint8_t {
    text,
    json,
};

constexpr auto log_basename(std::string_view path) noexcept -> std::string_view
{
    auto pos = std::string_view::npos;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/' || path[i] == '\\') {
            pos = i;
        }
    }

    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

struct log_context
{
    level lvl{ };
    std::string_view message;
    std::chrono::nanoseconds time{ };
    pid_t pid{ };
    int errno_{ 0 };
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    std::string_view file;
    std::string_view function;
    int line{ };
#endif
};

auto to_syslog_priority(level lvl) noexcept -> int;
auto level_name(level lvl) noexcept -> const char *;

} // namespace linyaps_box::log
