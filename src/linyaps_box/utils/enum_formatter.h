// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <fmt/format.h>

#include <algorithm>

// The single {fmt} integration point for enums and enum entries.

namespace linyaps_box::utils {

namespace detail {

struct enum_entry_view
{
    std::string_view name;
    uint64_t value;
};

template <typename E, std::size_t N>
constexpr auto make_enum_data(const enum_table<E, N> &table)
{
    struct packed_view
    {
        std::string_view type_name;
        std::array<enum_entry_view, N> views;
    };

    packed_view result{ table.type_name(), { } };
    for (std::size_t i = 0; i < N; ++i) {
        result.views[i] = { table.entries()[i].name,
                            static_cast<uint64_t>(table.entries()[i].value) };
    }

    return result;
}

template <typename T>
struct enum_or_bitflags_traits
{
    using enum_type = T;
    using underlying_type = enum_underlying_t<T>;

    static constexpr auto to_raw(T val) noexcept -> underlying_type
    {
        return static_cast<underlying_type>(val);
    }
};

template <typename E>
struct enum_or_bitflags_traits<bitflags<E>>
{
    using enum_type = E;
    using underlying_type = typename bitflags<E>::underlying_type;

    static constexpr auto to_raw(bitflags<E> val) noexcept -> underlying_type
    {
        return val.to_raw();
    }
};

template <typename OutputIt>
OutputIt format_flags_impl(OutputIt out,
                           uint64_t val,
                           std::string_view type_name,
                           const enum_entry_view *entries,
                           std::size_t count)
{
    if (val == 0) {
        for (std::size_t i = 0; i < count; ++i) {
            if (entries[i].value == 0) {
                return std::copy(entries[i].name.cbegin(), entries[i].name.cend(), out);
            }
        }

        static constexpr std::string_view none_sv = "none";
        return std::copy(none_sv.cbegin(), none_sv.cend(), out);
    }

    out = std::copy(type_name.cbegin(), type_name.cend(), out);
    *out++ = '(';

    bool first{ true };
    for (std::size_t i = 0; i < count; ++i) {
        const auto mask = entries[i].value;
        if (mask != 0 && (val & mask) == mask) {
            if (!first) {
                *out++ = '|';
            }

            out = std::copy(entries[i].name.cbegin(), entries[i].name.cend(), out);
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

    *out++ = ')';
    return out;
}

} // namespace detail

template <typename E>
constexpr auto format_as(const enum_entry<E> &entry) noexcept -> std::string_view
{
    return entry.name;
}

} // namespace linyaps_box::utils

template <typename T>
struct fmt::formatter<T, std::enable_if_t<linyaps_box::utils::has_enum_table_v<T>, char>>
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

        if (it != end && *it != '}') {
            throw fmt::format_error("invalid format specifier for enum");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const T &value, FormatContext &ctx) const
    {
        using Traits = linyaps_box::utils::detail::enum_or_bitflags_traits<T>;
        using E = typename Traits::enum_type;
        using U = typename Traits::underlying_type;
        using raw_type = std::conditional_t<std::is_same_v<U, bool>, unsigned, U>;

        const auto raw_val = static_cast<raw_type>(Traits::to_raw(value));

        if (presentation == Presentation::Hex) {
            using UnsignedU = std::make_unsigned_t<raw_type>;
            return fmt::format_to(ctx.out(), "0x{:x}", static_cast<UnsignedU>(raw_val));
        }

        if (presentation == Presentation::Decimal) {
            return fmt::format_to(ctx.out(), "{}", raw_val);
        }

        if constexpr (linyaps_box::utils::is_bitmask_enum_v<E>) {
            static constexpr auto enum_data =
              linyaps_box::utils::detail::make_enum_data(linyaps_box::utils::enum_table_v<E>);

            return linyaps_box::utils::detail::format_flags_impl(ctx.out(),
                                                                 static_cast<uint64_t>(raw_val),
                                                                 enum_data.type_name,
                                                                 enum_data.views.data(),
                                                                 enum_data.views.size());
        } else {
            if (auto name = linyaps_box::utils::enum_table_v<E>.to_name(static_cast<E>(raw_val))) {
                return std::copy(name->cbegin(), name->cend(), ctx.out());
            }

            return fmt::format_to(ctx.out(), "{}", raw_val);
        }
    }
};
