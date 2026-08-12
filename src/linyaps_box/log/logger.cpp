// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/logger.h"

#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/utils.h"

#include <chrono>

#include <unistd.h>

namespace linyaps_box::log {

auto make_spec(std::string_view log_dest) -> sink_spec
{
    constexpr auto file_log_flag = O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC;
    constexpr auto file_log_mod = 0600;
    auto idx = log_dest.find(':');
    if (idx == std::string_view::npos) {
        if (log_dest == "stderr") {
            return stderr_spec{ };
        }

        return file_spec{ linyaps_box::os::throw_if_error(
          linyaps_box::os::open(std::filesystem::path{ log_dest },
                                linyaps_box::os::throw_if_error(
                                  linyaps_box::os::sys::open_option::from_raw(file_log_flag)),
                                static_cast<std::filesystem::perms>(file_log_mod))) };
    }

    auto scheme = log_dest.substr(0, idx);
    auto content = log_dest.substr(idx + 1);

    if (scheme == "file") {
        return file_spec{ linyaps_box::os::throw_if_error(
          linyaps_box::os::open(std::filesystem::path{ content },
                                linyaps_box::os::throw_if_error(
                                  linyaps_box::os::sys::open_option::from_raw(file_log_flag)),
                                static_cast<std::filesystem::perms>(file_log_mod))) };
    }

    if (scheme == "syslog") {
        return syslog_spec{ std::string{ content } };
    }

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
    if (scheme == "journald") {
        return journald_spec{ std::string{ content } };
    }
#endif

    throw std::runtime_error(fmt::format("unknown log scheme: {}", log_dest.substr(0, idx)));
}

auto make_sink(sink_spec spec) noexcept -> sink_variant
{
    return std::visit(utils::Overload{
                        [](stderr_spec s) -> sink_variant {
                            return stderr_sink{ s };
                        },
                        [](file_spec s) -> sink_variant {
                            return file_sink{ std::move(s) };
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

auto get_current_log_level() noexcept -> level
{
    return global_logger::instance().get_level();
}

auto dispatch(level lvl,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void
{
    global_logger::instance().dispatch_log(lvl, fmt_str, args, file, function, line);
}

auto dispatch(level lvl,
              int errno_val,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void
{
    global_logger::instance().dispatch_log(lvl, fmt_str, args, file, function, line, errno_val);
}

auto global_logger::instance() noexcept -> global_logger &
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
    sinks_.swap(sinks);
}

auto global_logger::set_sink(sink_variant sink) noexcept -> void
{
    // for exception safety
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
                                 int line,
                                 int errno_val) const noexcept
{
    if (UNLIKELY(lvl > level_)) {
        return;
    }

    thread_local fmt::memory_buffer buf;
    buf.clear();
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

auto global_logger::dispatch_to_sinks(const log_context &ctx) const noexcept -> void
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

void global_logger::dispatch_raw(const log_context &ctx) const noexcept
{
    if (UNLIKELY(ctx.lvl > level_)) {
        return;
    }

    dispatch_to_sinks(ctx);
}

} // namespace linyaps_box::log
