// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"

#include <fmt/format.h>
#include <fmt/std.h>

#include <string_view>

namespace linyaps_box::log {

auto dispatch(level lvl,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void;

[[nodiscard]] auto get_current_log_level() noexcept -> level;

auto dispatch(level lvl,
              int errno_val,
              fmt::string_view fmt_str,
              fmt::format_args args,
              std::string_view file,
              std::string_view function,
              int line) noexcept -> void;

template <typename... Args>
inline auto fatal(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args) noexcept -> void
{
    dispatch(level::fatal, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline auto fatal(std::string_view file,
                  std::string_view function,
                  int line,
                  int errno_val,
                  fmt::format_string<Args...> fmt,
                  Args &&...args) noexcept -> void
{
    dispatch(level::fatal,
             errno_val,
             fmt.get(),
             fmt::make_format_args(args...),
             file,
             function,
             line);
}

template <typename... Args>
inline auto error(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args) noexcept -> void
{
    dispatch(level::error, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline auto error(std::string_view file,
                  std::string_view function,
                  int line,
                  int errno_val,
                  fmt::format_string<Args...> fmt,
                  Args &&...args) noexcept -> void
{
    dispatch(level::error,
             errno_val,
             fmt.get(),
             fmt::make_format_args(args...),
             file,
             function,
             line);
}

template <typename... Args>
inline auto warn(std::string_view file,
                 std::string_view function,
                 int line,
                 fmt::format_string<Args...> fmt,
                 Args &&...args) noexcept -> void
{
    dispatch(level::warn, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline auto warn(std::string_view file,
                 std::string_view function,
                 int line,
                 int errno_val,
                 fmt::format_string<Args...> fmt,
                 Args &&...args) noexcept -> void
{
    dispatch(level::warn,
             errno_val,
             fmt.get(),
             fmt::make_format_args(args...),
             file,
             function,
             line);
}

template <typename... Args>
inline auto info(std::string_view file,
                 std::string_view function,
                 int line,
                 fmt::format_string<Args...> fmt,
                 Args &&...args) noexcept -> void
{
    dispatch(level::info, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

template <typename... Args>
inline auto debug(std::string_view file,
                  std::string_view function,
                  int line,
                  fmt::format_string<Args...> fmt,
                  Args &&...args) noexcept -> void
{
    dispatch(level::debug, fmt.get(), fmt::make_format_args(args...), file, function, line);
}

} // namespace linyaps_box::log
