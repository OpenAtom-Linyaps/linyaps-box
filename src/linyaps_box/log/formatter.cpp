// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/formatter.h"

#include "linyaps_box/log/utils.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace linyaps_box::log {

auto to_json(nlohmann::json &j, level lvl) -> void
{
    switch (lvl) {
    case level::fatal:
        j = "FATAL";
        break;
    case level::error:
        j = "ERROR";
        break;
    case level::warn:
        j = "WARN";
        break;
    case level::info:
        j = "INFO";
        break;
    case level::debug:
        j = "DEBUG";
        break;
    }
}

auto from_json(const nlohmann::json &j, level &lvl) -> void
{
    auto s = j.get<std::string_view>();
    if (s == "FATAL") {
        lvl = level::fatal;
        return;
    }

    if (s == "ERROR") {
        lvl = level::error;
        return;
    }

    if (s == "WARN") {
        lvl = level::warn;
        return;
    }

    if (s == "INFO") {
        lvl = level::info;
        return;
    }

    if (s == "DEBUG") {
        lvl = level::debug;
        return;
    }

    throw std::invalid_argument(std::string{ "unknown log level: " }.append(s));
}

namespace detail {
struct oci_log_message
{
    std::string_view msg;
    static constexpr std::string_view base_indent = "    ";
};
} // namespace detail

namespace {

auto append_strerror(fmt::memory_buffer &buf, int errno_val) noexcept -> void
{
    if (errno_val != 0) {
        fmt::format_to(std::back_inserter(buf), "\n{}", detail::oci_log_message::base_indent);
        fmt::format_to(std::back_inserter(buf), "{}", ::strerror(errno_val));
    }
}

} // namespace

auto to_json(nlohmann::json &j, const log_context &ctx) noexcept -> void
{
    std::string time_str;
    time_str.reserve(32);
    fmt::format_to(std::back_inserter(time_str),
                   "{:%Y-%m-%dT%H:%M:%S}.{:09d}Z",
                   ctx.utc_tm(),
                   subsec_ns(ctx.time).count());

    j = nlohmann::json{
        { "time", time_str },
        { "level", ctx.lvl },
        { "pid", ctx.pid },
        { "msg", ctx.message },
    };
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    j["file"] = ctx.file;
    j["line"] = ctx.line;
    j["function"] = ctx.function;
#endif
    if (ctx.errno_ != 0) {
        j["errno"] = ctx.errno_;
        j["strerror"] = ::strerror(ctx.errno_);
    }
}

auto format_text(fmt::memory_buffer &buf, const log_context &ctx, fmt::text_style style) -> void
{
    // Format time into a local buffer first to avoid writing raw bytes to output
    thread_local fmt::memory_buffer time_buf;
    time_buf.clear();
    fmt::format_to(std::back_inserter(time_buf),
                   "{:%Y-%m-%dT%H:%M:%S}.{:09d}Z",
                   ctx.utc_tm(),
                   subsec_ns(ctx.time).count());

#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    fmt::format_to(std::back_inserter(buf),
                   style,
                   "[{}] [{:<5}] [{}] [{}:{} {}]:\n{}",
                   std::string_view{ time_buf.data(), time_buf.size() },
                   level_name(ctx.lvl),
                   ctx.pid,
                   ctx.file,
                   ctx.line,
                   ctx.function,
                   detail::oci_log_message{ ctx.message });
#else
    fmt::format_to(std::back_inserter(buf),
                   style,
                   "[{}] [{:<5}] [{}]:\n{}",
                   std::string_view{ time_buf.data(), time_buf.size() },
                   level_name(ctx.lvl),
                   ctx.pid,
                   detail::oci_log_message{ ctx.message });
#endif
    append_strerror(buf, ctx.errno_);

    buf.push_back('\n');
}

auto format_log(fmt::memory_buffer &buf,
                const log_context &ctx,
                output_format fmt,
                fmt::text_style style) noexcept -> void
{
    switch (fmt) {
    case output_format::text:
        format_text(buf, ctx, style);
        break;
    case output_format::json:
        fmt::format_to(std::back_inserter(buf), "{}\n", nlohmann::json(ctx).dump());
        break;
    }
}

} // namespace linyaps_box::log

namespace fmt {
template <>
struct formatter<linyaps_box::log::detail::oci_log_message>
{
    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (it != end && *it != '}') {
            throw fmt::format_error("oci_log_message does not accept format specs");
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const linyaps_box::log::detail::oci_log_message &lm, FormatContext &ctx) const
    {
        auto out = ctx.out();
        auto text = lm.msg;

        while (!text.empty() && text.back() == '\n') {
            text.remove_suffix(1);
        }

        out = fmt::format_to(out, "{}", linyaps_box::log::detail::oci_log_message::base_indent);

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
                                 linyaps_box::log::detail::oci_log_message::base_indent);
            start = pos + 1;
        }

        return out;
    }
};
} // namespace fmt
