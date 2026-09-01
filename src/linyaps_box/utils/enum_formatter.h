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

namespace linyaps_box::utils::detail {

struct enum_entry_view
{
    std::string_view name;
    uint64_t value;
};

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

    std::copy(type_name.cbegin(), type_name.cend(), out);
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

} // namespace linyaps_box::utils::detail

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

        const U raw_val = Traits::to_raw(value);

        if (presentation == Presentation::Hex) {
            using UnsignedU = std::make_unsigned_t<U>;
            return fmt::format_to(ctx.out(), "0x{:x}", static_cast<UnsignedU>(raw_val));
        }

        if (presentation == Presentation::Decimal) {
            return fmt::format_to(ctx.out(), "{}", raw_val);
        }

        if constexpr (linyaps_box::utils::is_bitmask_enum_v<E>) {
            static constexpr auto enum_data = [] {
                constexpr auto table = get_enum_table(static_cast<E *>(nullptr));

                struct packed_view
                {
                    std::string_view type_name;
                    std::array<linyaps_box::utils::detail::enum_entry_view, table.entries().size()>
                      views;
                };

                packed_view result{ table.type_name(), { } };
                for (std::size_t i = 0; i < table.entries().size(); ++i) {
                    result.views[i] = { table.entries()[i].name,
                                        static_cast<uint64_t>(table.entries()[i].value) };
                }

                return result;
            }();

            return linyaps_box::utils::detail::format_flags_impl(ctx.out(),
                                                                 static_cast<uint64_t>(raw_val),
                                                                 enum_data.type_name,
                                                                 enum_data.views.data(),
                                                                 enum_data.views.size());
        } else {
            constexpr auto table = get_enum_table(static_cast<E *>(nullptr));
            if (auto name = table.to_name(static_cast<E>(raw_val))) {
                return std::copy(name->cbegin(), name->cend(), ctx.out());
            }

            return fmt::format_to(ctx.out(), "{}", raw_val);
        }
    }
};
