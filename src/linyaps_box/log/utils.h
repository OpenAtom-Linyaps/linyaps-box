// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/format.h>

#include <chrono>
#include <cstdint>
#include <ctime>
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

constexpr std::string_view log_basename(std::string_view path) noexcept
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
    std::string msg;
    std::chrono::system_clock::time_point wall_time;
    pid_t pid{ };
    int errno_{ 0 };
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    std::string file;
    std::string function;
    int line{ };
#endif

    [[nodiscard]] std::tm utc_tm() const
    {
        auto t = std::chrono::system_clock::to_time_t(wall_time);
        std::tm tm{ };
        gmtime_r(&t, &tm);
        return tm;
    }
};

inline auto subsec_ns(std::chrono::system_clock::time_point tp) noexcept -> std::chrono::nanoseconds
{
    using namespace std::chrono;
    auto as_ns = duration_cast<nanoseconds>(tp.time_since_epoch());
    auto secs = duration_cast<seconds>(as_ns);
    return as_ns - secs;
}

auto to_syslog_priority(level lvl) noexcept -> int;
auto level_name(level lvl) noexcept -> const char *;

struct OciLogMessage
{
    std::string_view msg;
    static constexpr std::string_view base_indent = "    ";
};

} // namespace linyaps_box::log

template <>
struct fmt::formatter<linyaps_box::log::OciLogMessage>
{
    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (it != end && *it != '}') {
            throw fmt::format_error("OciLogMessage does not accept format specs");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const linyaps_box::log::OciLogMessage &lm, FormatContext &ctx) const
    {
        auto out = ctx.out();
        auto text = lm.msg;

        while (!text.empty() && text.back() == '\n') {
            text.remove_suffix(1);
        }

        out = fmt::format_to(out, "{}", linyaps_box::log::OciLogMessage::base_indent);

        std::string_view::size_type start = 0;
        while (true) {
            auto pos = text.find('\n', start);
            if (pos == std::string_view::npos) {
                out = fmt::format_to(out, "{}", text.substr(start));
                break;
            }

            out = fmt::format_to(out,
                                 "{}\n{}",
                                 text.substr(start, pos - start),
                                 linyaps_box::log::OciLogMessage::base_indent);
            start = pos + 1;
        }

        return out;
    }
};
