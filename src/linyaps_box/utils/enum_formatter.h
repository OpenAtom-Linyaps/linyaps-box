// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <fmt/format.h>

#include <algorithm>

// Out-of-line definitions of the fmt-dependent methods of enum_table.
// Kept separate from enum_traits.h so that headers which only need the
// bitmask / enum-table infrastructure (e.g. config.h) do not pull in
// <fmt/format.h>.

template <typename E, std::size_t N>
template <typename OutputIt>
OutputIt linyaps_box::utils::enum_table<E, N>::format_to(OutputIt out, E value) const
{
    if constexpr (is_bitmask_enum_v<E>) {
        return format_flags_to(out, value);
    } else {
        return format_single_to(out, value);
    }
}

template <typename E, std::size_t N>
template <typename OutputIt>
OutputIt linyaps_box::utils::enum_table<E, N>::format_single_to(OutputIt out, E value) const
{
    if (auto name = to_name(value)) {
        return std::copy(name->begin(), name->end(), out);
    }

    using U = std::underlying_type_t<E>;
    return fmt::format_to(out, "{}", static_cast<U>(value));
}

template <typename E, std::size_t N>
template <typename OutputIt>
OutputIt linyaps_box::utils::enum_table<E, N>::format_flags_to(OutputIt out, E flags) const
{
    using U = std::underlying_type_t<E>;
    auto val = static_cast<U>(flags);

    if (val == 0) {
        if (auto name = to_name(flags)) {
            return std::copy(name->begin(), name->end(), out);
        }

        static constexpr std::string_view none_sv = "none";
        return std::copy(none_sv.begin(), none_sv.end(), out);
    }

    bool first = true;
    for (const auto &entry : entries) {
        const auto mask = static_cast<U>(entry.value);
        if (mask != 0 && (val & mask) == mask) {
            if (!first) {
                *out++ = '|';
            }

            out = std::copy(entry.name.begin(), entry.name.end(), out);
            first = false;
            val &= ~mask;

            if (val == 0) {
                break;
            }
        }
    }

    if (val != 0) {
        if (!first) {
            *out++ = '|';
        }

        out = fmt::format_to(out, "0x{:x}", val);
    }

    return out;
}

template <typename E>
struct fmt::formatter<E, std::enable_if_t<linyaps_box::utils::has_enum_table_v<E>, char>>
{
    enum class Presentation : uint8_t { Default, Hex, Decimal };
    Presentation presentation{ Presentation::Default };

    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (it != end && *it != '}') {
            if (*it == 'x') {
                presentation = Presentation::Hex;
            } else if (*it == 'd') {
                presentation = Presentation::Decimal;
            } else {
                throw fmt::format_error("invalid format specifier for enum");
            }

            ++it;
        }

        return it;
    }

    template <typename FormatContext>
    auto format(E value, FormatContext &ctx) const
    {
        using U = std::underlying_type_t<E>;
        if (presentation == Presentation::Hex) {
            return fmt::format_to(ctx.out(), "0x{:x}", static_cast<U>(value));
        }

        if (presentation == Presentation::Decimal) {
            return fmt::format_to(ctx.out(), "{}", static_cast<U>(value));
        }

        constexpr auto table = get_enum_table(static_cast<E *>(nullptr));
        return table.format_to(ctx.out(), value);
    }
};
