// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/formatter.h"

#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/utils.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace linyaps_box::log {

void to_json(nlohmann::json &j, level lvl)
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

void from_json(const nlohmann::json &j, level &lvl)
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

namespace {

void append_strerror(fmt::memory_buffer &buf, int errno_val)
{
    if (errno_val != 0) {
        fmt::format_to(std::back_inserter(buf), "\n{}", OciLogMessage::base_indent);
        fmt::format_to(std::back_inserter(buf), "{}", ::strerror(errno_val));
    }
}

} // namespace

void to_json(nlohmann::json &j, const log_context &ctx)
{
    std::string time_str;
    time_str.reserve(32);
    fmt::format_to(std::back_inserter(time_str),
                   "{:%Y-%m-%dT%H:%M:%S}.{:09d}Z",
                   ctx.utc_tm(),
                   subsec_ns(ctx.wall_time).count());

    j = nlohmann::json{
        { "time", time_str },
        { "level", ctx.lvl },
        { "pid", ctx.pid },
        { "msg", ctx.msg },
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

auto log_context_to_json_string(const log_context &ctx) -> std::string
{
    return nlohmann::json(ctx).dump();
}

void format_text(fmt::memory_buffer &buf, const log_context &ctx, fmt::text_style style)
{
    // Format time into a local buffer first to avoid writing raw bytes to output
    thread_local fmt::memory_buffer time_buf;
    time_buf.clear();
    fmt::format_to(std::back_inserter(time_buf),
                   "{:%Y-%m-%dT%H:%M:%S}.{:09d}Z",
                   ctx.utc_tm(),
                   subsec_ns(ctx.wall_time).count());

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
                   OciLogMessage{ ctx.msg });
#else
    fmt::format_to(std::back_inserter(buf),
                   style,
                   "[{}] [{:<5}] [{}]:\n{}",
                   std::string_view{ time_buf.data(), time_buf.size() },
                   level_name(ctx.lvl),
                   ctx.pid,
                   OciLogMessage{ ctx.msg });
#endif
    append_strerror(buf, ctx.errno_);

    buf.push_back('\n');
}

void format_log(fmt::memory_buffer &buf,
                const log_context &ctx,
                output_format fmt,
                fmt::text_style style)
{
    switch (fmt) {
    case output_format::text:
        format_text(buf, ctx, style);
        break;
    case output_format::json:
        fmt::format_to(std::back_inserter(buf), "{}", log_context_to_json_string(ctx));
        break;
    }
}

auto get_current_format() noexcept -> output_format
{
    return global_logger::instance().get_format();
}

} // namespace linyaps_box::log
