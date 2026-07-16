// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/logger.h"

#include "linyaps_box/utils/utils.h"

#include <chrono>

#include <unistd.h>

namespace linyaps_box::log {

auto parse_log_to(std::string_view spec) -> sink_spec
{
    auto idx = spec.find(':');
    if (idx == std::string_view::npos) {
        if (spec == "stderr") {
            return stderr_spec{ };
        }

        return file_spec{ spec };
    }

    auto scheme = spec.substr(0, idx);
    auto content = spec.substr(idx + 1);

    if (scheme == "file") {
        return file_spec{ content };
    }

    if (scheme == "syslog") {
        return syslog_spec{ std::string{ content } };
    }

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
    if (scheme == "journald") {
        return journald_spec{ std::string{ content } };
    }
#endif

    throw std::runtime_error(fmt::format("unknown log scheme: {}", spec.substr(0, idx)));
}

auto make_sink(sink_spec spec) -> sink_variant
{
    return std::visit(utils::Overload{
                        [](stderr_spec s) -> sink_variant {
                            return stderr_sink{ s };
                        },
                        [](const file_spec &s) -> sink_variant {
                            return file_sink{ s };
                        },
                        [](syslog_spec s) -> sink_variant {
                            return syslog_sink{ std::move(s) };
                        },
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
                        [](journald_spec s) -> sink_variant {
                            return journald_sink{ std::move(s) };
                        },
#endif
                      },
                      std::move(spec));
}

global_logger &global_logger::instance() noexcept
{
    static global_logger logger;
    return logger;
}

auto global_logger::set_level(level lvl) noexcept -> void
{
    level_ = lvl;
}

auto global_logger::get_level() const noexcept -> level
{
    return level_;
}

auto global_logger::set_format(output_format fmt) noexcept -> void
{
    format_ = fmt;
}

auto global_logger::get_format() const noexcept -> output_format
{
    return format_;
}

auto global_logger::set_sinks(std::vector<sink_variant> sinks) noexcept -> void
{
    sinks_ = std::move(sinks);
}

auto global_logger::set_sink(sink_variant sink) -> void
{
    std::vector<sink_variant> tmp;
    tmp.push_back(std::move(sink));
    sinks_.swap(tmp);
}

auto global_logger::unset_sink() noexcept -> void
{
    sinks_.clear();
}

void global_logger::dispatch_log(level lvl,
                                 fmt::string_view fmt_str,
                                 fmt::format_args args,
                                 std::string_view file,
                                 std::string_view function,
                                 int line) const
{
    if (UNLIKELY(lvl > level_)) {
        return;
    }

    fmt::memory_buffer buf;
    fmt::vformat_to(std::back_inserter(buf), fmt::locale_ref{ }, fmt_str, args);

    const log_context ctx{
        lvl,
        std::string_view{ buf.data(), buf.size() },
        std::chrono::system_clock::now(),
        ::getpid(),
        0,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        file,
        function,
        line,
#endif
    };
    dispatch_to_sinks(ctx);
}

void global_logger::dispatch_log(level lvl,
                                 int errno_val,
                                 fmt::string_view fmt_str,
                                 fmt::format_args args,
                                 std::string_view file,
                                 std::string_view function,
                                 int line) const
{
    if (UNLIKELY(lvl > level_)) {
        return;
    }

    fmt::memory_buffer buf;
    fmt::vformat_to(std::back_inserter(buf), fmt::locale_ref{ }, fmt_str, args);

    const log_context ctx{
        lvl,
        std::string_view{ buf.data(), buf.size() },
        std::chrono::system_clock::now(),
        ::getpid(),
        errno_val,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        file,
        function,
        line,
#endif
    };
    dispatch_to_sinks(ctx);
}

auto global_logger::dispatch_to_sinks(const log_context &ctx) const -> void
{
    for (const auto &sink : sinks_) {
        std::visit(utils::Overload{
                     [](std::monostate) {
                         // no sink installed — drop
                     },
                     [&](const auto &s) {
                         s.log(ctx);
                     },
                   },
                   sink);
    }
}

void global_logger::dispatch_raw(const log_context &ctx) const
{
    if (UNLIKELY(ctx.lvl > level_)) {
        return;
    }

    dispatch_to_sinks(ctx);
}

} // namespace linyaps_box::log
