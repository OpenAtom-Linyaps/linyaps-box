// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/std.h>

#include <cstdint>
#include <string_view>

namespace linyaps_box::protocol::stage {

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

} // namespace linyaps_box::protocol::stage

template <>
struct fmt::formatter<linyaps_box::protocol::stage::type> : fmt::formatter<std::string>
{
    auto format(linyaps_box::protocol::stage::type s, fmt::format_context &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", linyaps_box::protocol::stage::to_string_view(s));
    }
};
