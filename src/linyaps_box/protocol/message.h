// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"

#include <fmt/std.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <sys/types.h>

namespace linyaps_box::protocol {

enum class msg_id : uint8_t {
    log,
    stage,
    pid_report,
    console_fd,
    proceed,
};

namespace stage {

enum class type : uint8_t {
    namespace_ready,
    namespace_done,
    prestart_ready,
    prestart_done,
    createruntime_ready,
    createruntime_done,
    createcontainer_done,
    exec_ready,
};

[[nodiscard]] inline auto to_string_view(type s) noexcept -> std::string_view
{
    using namespace std::string_view_literals;
    switch (s) {
    case type::namespace_ready:
        return "namespace_ready"sv;
    case type::namespace_done:
        return "namespace_done"sv;
    case type::prestart_ready:
        return "prestart_ready"sv;
    case type::prestart_done:
        return "prestart_done"sv;
    case type::createruntime_ready:
        return "createruntime_ready"sv;
    case type::createruntime_done:
        return "createruntime_done"sv;
    case type::createcontainer_done:
        return "createcontainer_done"sv;
    case type::exec_ready:
        return "exec_ready"sv;
    }

    return "<invalid>"sv;
}

[[nodiscard]] constexpr auto is_valid(type s) noexcept -> bool
{
    switch (s) {
    case type::namespace_ready:
    case type::namespace_done:
    case type::prestart_ready:
    case type::prestart_done:
    case type::createruntime_ready:
    case type::createruntime_done:
    case type::createcontainer_done:
    case type::exec_ready:
        return true;
    }
    return false;
}

} // namespace stage

} // namespace linyaps_box::protocol

namespace linyaps_box::protocol::msg {

struct log
{
    std::string message;
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    std::string file;
    std::string function;
    int line{ };
#endif
    int errno_{ 0 };
    std::chrono::nanoseconds time{ };
    pid_t pid{ };
    linyaps_box::log::level lvl{ };
};

[[nodiscard]] auto to_log_context(const log &l) noexcept -> linyaps_box::log::log_context;

struct stage
{
    protocol::stage::type value{ };
};

struct pid_report
{
    pid_t value{ };
};

struct console_fd
{
};

struct proceed
{
};

using message = std::variant<log, stage, pid_report, console_fd, proceed>;

struct datagram
{
    message body;
    std::vector<utils::file_descriptor> fds;
    [[nodiscard]] auto take_fds() -> std::vector<utils::file_descriptor>;
};

[[nodiscard]] auto serialize(const message &msg) -> std::vector<std::byte>;

[[nodiscard]] auto deserialize(utils::span<const std::byte> wire) -> message;

auto serialize_log_into(std::vector<std::byte> &buf, const linyaps_box::log::log_context &ctx)
  -> void;

} // namespace linyaps_box::protocol::msg

template <>
struct fmt::formatter<linyaps_box::protocol::msg::log> : fmt::formatter<std::string>
{
    auto format(const linyaps_box::protocol::msg::log &l, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(),
                              "log{{lvl={}, message=\"{}\""
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                              ", file={}, line={}, function={}"
#endif
                              ", errno={}, pid={}}}",
                              linyaps_box::log::level_name(l.lvl),
                              l.message,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                              l.file,
                              l.line,
                              l.function,
#endif
                              l.errno_,
                              l.pid);
    }
};

template <>
struct fmt::formatter<linyaps_box::protocol::msg::stage> : fmt::formatter<std::string>
{
    auto format(const linyaps_box::protocol::msg::stage &s, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "stage{{value={}}}", s.value);
    }
};

template <>
struct fmt::formatter<linyaps_box::protocol::msg::pid_report> : fmt::formatter<std::string>
{
    auto format(const linyaps_box::protocol::msg::pid_report &p, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "pid_report{{value={}}}", p.value);
    }
};

template <>
struct fmt::formatter<linyaps_box::protocol::msg::console_fd> : fmt::formatter<std::string>
{
    auto format([[maybe_unused]] const linyaps_box::protocol::msg::console_fd &p,
                fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "console_fd");
    }
};

template <>
struct fmt::formatter<linyaps_box::protocol::msg::proceed> : fmt::formatter<std::string>
{
    auto format([[maybe_unused]] const linyaps_box::protocol::msg::proceed &p,
                fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "proceed");
    }
};

template <>
struct fmt::formatter<linyaps_box::protocol::stage::type> : fmt::formatter<std::string>
{
    auto format(linyaps_box::protocol::stage::type s, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", linyaps_box::protocol::stage::to_string_view(s));
    }
};
