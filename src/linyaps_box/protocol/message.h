// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/protocol/stage.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"

#include <fmt/std.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <sys/types.h>

namespace linyaps_box::protocol {

enum class msg_id : uint8_t {
    die,
    stage,
    pid_report,
    console_fd,
    proceed,
};

} // namespace linyaps_box::protocol

namespace linyaps_box::protocol::msg {

struct die
{
    static constexpr msg_id id = msg_id::die;
    int errnum{ };
    std::string message;
};

struct stage
{
    static constexpr msg_id id = msg_id::stage;
    protocol::stage::type value{ };
};

struct pid_report
{
    static constexpr msg_id id = msg_id::pid_report;
    pid_t value{ };
};

struct console_fd
{
    static constexpr msg_id id = msg_id::console_fd;
};

struct proceed
{
    static constexpr msg_id id = msg_id::proceed;
};

using message = std::variant<die, stage, pid_report, console_fd, proceed>;

struct datagram
{
    message body;
    std::vector<utils::file_descriptor> fds;
    [[nodiscard]] auto take_fds() -> std::vector<utils::file_descriptor>;
};

[[nodiscard]] auto serialize(const message &msg) -> std::vector<std::byte>;
[[nodiscard]] auto deserialize(utils::span<const std::byte> wire) -> message;

} // namespace linyaps_box::protocol::msg

template <>
struct fmt::formatter<linyaps_box::protocol::msg::die> : fmt::formatter<std::string>
{
    auto format(const linyaps_box::protocol::msg::die &d, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "die{{errnum={}, message=\"{}\"}}", d.errnum, d.message);
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
