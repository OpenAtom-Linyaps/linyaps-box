// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/format.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace linyaps_box::utils {

template <typename E>
using enum_underlying_t = std::underlying_type_t<E>;

template <typename E, typename = void>
struct is_bitmask_enum : std::false_type
{
};

template <typename E>
struct is_bitmask_enum<E, std::void_t<decltype(enable_bitmask_enum(static_cast<E *>(nullptr)))>>
    : std::true_type
{
};

template <typename E>
inline constexpr bool is_bitmask_enum_v = is_bitmask_enum<E>::value;

template <typename E>
struct enum_entry
{
    E value;
    std::string_view name;
};

template <typename E>
enum_entry(E, std::string_view) -> enum_entry<E>;

template <typename E, std::size_t N>
struct enum_table
{
    std::array<enum_entry<E>, N> entries;

    [[nodiscard]] constexpr auto to_name(E value) const noexcept -> std::optional<std::string_view>
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

    template <typename OutputIt>
    OutputIt format_to(OutputIt out, E value) const
    {
        if constexpr (is_bitmask_enum_v<E>) {
            return format_flags_to(out, value);
        } else {
            return format_single_to(out, value);
        }
    }

private:
    template <typename OutputIt>
    OutputIt format_single_to(OutputIt out, E value) const
    {
        if (auto name = to_name(value)) {
            return std::copy(name->begin(), name->end(), out);
        }

        using U = std::underlying_type_t<E>;
        return fmt::format_to(out, "{}", static_cast<U>(value));
    }

    template <typename OutputIt>
    OutputIt format_flags_to(OutputIt out, E flags) const
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
};

template <typename E, typename... Rest>
enum_table(enum_entry<E>, Rest...) -> enum_table<E, 1 + sizeof...(Rest)>;

template <typename E, typename = void>
struct has_enum_table : std::false_type
{
};

template <typename E>
struct has_enum_table<E, std::void_t<decltype(get_enum_table(static_cast<E *>(nullptr)))>>
    : std::true_type
{
};

template <typename E>
inline constexpr bool has_enum_table_v = has_enum_table<E>::value;

template <typename E, std::size_t N>
constexpr auto make_enum_table(const enum_entry<E> (&entries)[N]) noexcept -> enum_table<E, N>
{
    enum_table<E, N> table{ };
    for (std::size_t i = 0; i < N; ++i) {
        table.entries[i] = entries[i];
    }
    return table;
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

} // namespace linyaps_box::utils

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

#define ENABLE_BITMASK_OPERATORS(E)                                       \
    constexpr inline std::true_type enable_bitmask_enum(E *) noexcept     \
    {                                                                     \
        return { };                                                       \
    }                                                                     \
    [[nodiscard]] constexpr E operator|(E lhs, E rhs) noexcept            \
    {                                                                     \
        using U = std::underlying_type_t<E>;                              \
        return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs)); \
    }                                                                     \
    [[nodiscard]] constexpr E operator&(E lhs, E rhs) noexcept            \
    {                                                                     \
        using U = std::underlying_type_t<E>;                              \
        return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs)); \
    }                                                                     \
    [[nodiscard]] constexpr E operator^(E lhs, E rhs) noexcept            \
    {                                                                     \
        using U = std::underlying_type_t<E>;                              \
        return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs)); \
    }                                                                     \
    [[nodiscard]] constexpr E operator~(E rhs) noexcept                   \
    {                                                                     \
        using U = std::underlying_type_t<E>;                              \
        return static_cast<E>(~static_cast<U>(rhs));                      \
    }                                                                     \
    constexpr E &operator|=(E &lhs, E rhs) noexcept                       \
    {                                                                     \
        return lhs = lhs | rhs;                                           \
    }                                                                     \
    constexpr E &operator&=(E &lhs, E rhs) noexcept                       \
    {                                                                     \
        return lhs = lhs & rhs;                                           \
    }                                                                     \
    constexpr E &operator^=(E &lhs, E rhs) noexcept                       \
    {                                                                     \
        return lhs = lhs ^ rhs;                                           \
    }

#define LINYAPS_REGISTER_ENUM(E, ...)                                                     \
    constexpr inline auto get_enum_table(E *) noexcept                                    \
    {                                                                                     \
        constexpr auto table = ::linyaps_box::utils::make_enum_table<E>({ __VA_ARGS__ }); \
        static_assert(::linyaps_box::utils::verify_enum_table(table),                     \
                      "enum_table validation failed for " #E                              \
                      ": duplicate name or value detected!");                             \
        return table;                                                                     \
    }

#define LINYAPS_REGISTER_BITMASK_ENUM(E, ...) \
    ENABLE_BITMASK_OPERATORS(E)               \
    LINYAPS_REGISTER_ENUM(E, __VA_ARGS__)
