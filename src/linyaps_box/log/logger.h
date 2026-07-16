// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/sink.h"

#include <fmt/format.h>

#include <vector>

namespace linyaps_box::log {

// NOTE: The global logger state (level + sinks) is NOT thread-safe.
// The runtime operates in a multi-process single-threaded model: each
// process (parent, child) has its own copy of the global state after
// clone/fork, and within each process all logging happens on one thread.
// If multi-threaded logging is needed in the future, a mutex or lock-free
// approach must be added here.
class global_logger
{
public:
    global_logger(const global_logger &) = delete;
    global_logger(global_logger &&) noexcept = delete;
    global_logger &operator=(const global_logger &) = delete;
    global_logger &operator=(global_logger &&) = delete;
    ~global_logger() noexcept = default;

    static global_logger &instance() noexcept;

    auto set_level(level) noexcept -> void;
    [[nodiscard]] auto get_level() const noexcept -> level;

    auto set_format(output_format fmt) noexcept -> void;
    [[nodiscard]] auto get_format() const noexcept -> output_format;

    auto set_sinks(std::vector<sink_variant> sinks) noexcept -> void;
    auto set_sink(sink_variant sink) -> void;
    auto unset_sink() noexcept -> void;

    void dispatch_log(level lvl,
                      fmt::string_view fmt_str,
                      fmt::format_args args,
                      std::string_view file,
                      std::string_view function,
                      int line) const;

    void dispatch_log(level lvl,
                      int errno_val,
                      fmt::string_view fmt_str,
                      fmt::format_args args,
                      std::string_view file,
                      std::string_view function,
                      int line) const;

    void dispatch_to_sinks(const log_context &ctx) const;

    void dispatch_raw(const log_context &ctx) const;

private:
    global_logger() noexcept = default;

    std::vector<sink_variant> sinks_;
    level level_{ level::warn };
    output_format format_{ output_format::text };
};

using sink_spec = std::variant<stderr_spec,
                               file_spec,
                               syslog_spec
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
                               ,
                               journald_spec
#endif
                               >;

auto parse_log_to(std::string_view spec) -> sink_spec;
auto make_sink(sink_spec spec) -> sink_variant;

template <typename... Args>
inline void fatal(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args)
{
    global_logger::instance()
      .dispatch_log(level::fatal, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline void fatal(std::string_view file,
                  std::string_view function,
                  int line,
                  int errno_val,
                  fmt::format_string<Args...> fmt,
                  Args &&...args)
{
    global_logger::instance().dispatch_log(level::fatal,
                                           errno_val,
                                           fmt.get(),
                                           fmt::make_format_args(args...),
                                           file,
                                           function,
                                           line);
}

template <typename... Args>
inline void error(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args)
{
    global_logger::instance()
      .dispatch_log(level::error, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline void error(std::string_view file,
                  std::string_view function,
                  int line,
                  int errno_val,
                  fmt::format_string<Args...> fmt,
                  Args &&...args)
{
    global_logger::instance().dispatch_log(level::error,
                                           errno_val,
                                           fmt.get(),
                                           fmt::make_format_args(args...),
                                           file,
                                           function,
                                           line);
}

template <typename... Args>
inline void warn(std::string_view file,
                 std::string_view function,
                 int line,
                 fmt::format_string<Args...> fmt,
                 Args &&...args)
{
    global_logger::instance()
      .dispatch_log(level::warn, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline void info(std::string_view file,
                 std::string_view function,
                 int line,
                 fmt::format_string<Args...> fmt,
                 Args &&...args)
{
    global_logger::instance()
      .dispatch_log(level::info, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline void debug(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args)
{
    global_logger::instance()
      .dispatch_log(level::debug, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

} // namespace linyaps_box::log
