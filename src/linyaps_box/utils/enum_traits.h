// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace linyaps_box::utils {

template <typename E>
using enum_underlying_t = std::underlying_type_t<E>;

template <typename E>
struct enum_entry
{
    E value;
    std::string_view name;
};

template <typename E, std::size_t N>
struct enum_table
{
    std::array<enum_entry<E>, N> entries;

    [[nodiscard]] constexpr auto to_name(const E &value) const noexcept
      -> std::optional<std::string_view>
    {
        for (const auto &entry : entries) {
            if (entry.value == value) {
                return entry.name;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] constexpr auto from_name(std::string_view name) const noexcept -> std::optional<E>
    {
        for (const auto &entry : entries) {
            if (entry.name == name) {
                return entry.value;
            }
        }

        return std::nullopt;
    }
};

template <typename E>
constexpr auto bitmask_popcount(E value) noexcept -> int
{
    using U = std::make_unsigned_t<std::underlying_type_t<E>>;
    return __builtin_popcountll(static_cast<unsigned long long>(static_cast<U>(value)));
}

template <typename E, std::size_t N, typename F>
constexpr void for_each_bit(E value, const enum_table<E, N> &table, const F &callback)
{
    static_assert(std::is_invocable_v<F, const enum_entry<E> &>);
    auto u = static_cast<enum_underlying_t<E>>(value);
    for (const auto &entry : table.entries) {
        auto eu = static_cast<enum_underlying_t<E>>(entry.value);
        if (eu != 0 && (u & eu) != 0) {
            callback(entry);
        }
    }
}

template <typename E, std::size_t N>
constexpr auto verify_enum_table(const enum_table<E, N> &table) noexcept -> bool
{
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (table.entries[i].name == table.entries[j].name
                || table.entries[i].value == table.entries[j].value) {
                return false;
            }
        }
    }

    return true;
}

template <typename E, std::size_t N, typename Pred>
constexpr auto verify_enum_table(const enum_table<E, N> &table, Pred is_valid) -> bool
{
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (!is_valid(table.entries[i], table.entries[j])) {
                return false;
            }
        }
    }

    return true;
}

template <typename T>
struct is_bitmask_enum : std::false_type
{
};

template <typename T>
inline constexpr bool is_bitmask_enum_v = is_bitmask_enum<T>::value;

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E> operator|(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E> operator&(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E> operator^(E lhs, E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E> operator~(E rhs) noexcept
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(rhs));
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E &> operator|=(E &lhs, E rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E &> operator&=(E &lhs, E rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

template <typename E>
constexpr std::enable_if_t<is_bitmask_enum_v<E>, E &> operator^=(E &lhs, E rhs) noexcept
{
    lhs = lhs ^ rhs;
    return lhs;
}

} // namespace linyaps_box::utils
