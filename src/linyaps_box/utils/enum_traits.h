// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/span.h"
#include "linyaps_box/utils/utils.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace linyaps_box::utils {

namespace detail {

template <typename T, std::size_t N, typename Compare>
constexpr void shell_sort(span<T, N> entries, Compare comp) noexcept
{
    for (const std::size_t gap : { 123, 54, 23, 10, 4, 1 }) {
        if (gap >= N) {
            continue;
        }

        for (auto i = gap; i < N; ++i) {
            auto temp = entries[i];
            auto j = i;

            while (j >= gap && comp(temp, entries[j - gap])) {
                entries[j] = entries[j - gap];
                j -= gap;
            }

            entries[j] = temp;
        }
    }
}

template <typename E, bool = std::is_enum_v<E>>
struct safe_underlying
{
    using type = E;
};

template <typename E>
struct safe_underlying<E, true>
{
    using type = std::underlying_type_t<E>;
};

template <typename E>
using enum_underlying_t = typename safe_underlying<E>::type;

template <typename T>
constexpr auto popcount(T val) noexcept -> int
{
    if constexpr (std::is_enum_v<T>) {
        using U = std::make_unsigned_t<enum_underlying_t<T>>;
        return detail::popcount(static_cast<U>(val));
    } else {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                      "popcount requires an integral or enum type (excluding bool)");

        using U = std::make_unsigned_t<T>;
        const auto uval = static_cast<U>(val);

        if constexpr (sizeof(U) <= sizeof(unsigned int)) {
            return __builtin_popcount(static_cast<unsigned int>(uval));
        } else if constexpr (sizeof(U) <= sizeof(unsigned long long)) {
            return __builtin_popcountll(static_cast<unsigned long long>(uval));
        } else {
            static_assert(sizeof(U) <= 16, "Types larger than 128-bit are not supported");
            const auto low = static_cast<unsigned long long>(uval);
            const auto high = static_cast<unsigned long long>(uval >> 64);
            return __builtin_popcountll(low) + __builtin_popcountll(high);
        }
    }
}

template <typename T>
constexpr auto enable_bitmask_enum_tag(T *) noexcept -> std::false_type;

template <typename E>
constexpr auto check_is_bitmask_enum() noexcept
{
    return decltype(enable_bitmask_enum_tag(static_cast<E *>(nullptr)))::value;
}

} // namespace detail

template <typename E>
inline constexpr bool is_bitmask_enum_v = detail::check_is_bitmask_enum<E>();

template <typename E>
struct enum_entry
{
    // GCC 8's constexpr evaluator requires the defaulted default constructor
    // to initialize every member; without `= {}` the implicit default ctor
    // leaves `value` uninitialized and is not usable in constant expressions.
    E value{ };
    std::string_view name;
};

template <typename E>
enum_entry(E, std::string_view) -> enum_entry<E>;

template <typename E, std::size_t N>
class enum_table;

template <typename E, std::size_t N>
constexpr auto make_enum_table(std::string_view type_name, const enum_entry<E> (&arr)[N]) noexcept
  -> enum_table<E, N>;

template <typename E, std::size_t N>
constexpr auto verify_enum_table(const enum_table<E, N> &entries) noexcept -> bool;

template <typename E, std::size_t N>
class enum_table
{
public:
    constexpr enum_table(std::string_view name, const enum_entry<E> (&arr)[N]) noexcept
        : type_name_(name)
        , entries_(utils::to_array(arr))
    {
    }

    [[nodiscard]] constexpr auto to_name(E value) const noexcept -> std::optional<std::string_view>
    {
        for (const auto &entry : entries_) {
            if (entry.value == value) {
                return entry.name;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] constexpr auto from_name(std::string_view name) const noexcept -> std::optional<E>
    {
        for (const auto &entry : entries_) {
            if (entry.name == name) {
                return entry.value;
            }
        }

        return std::nullopt;
    }

    friend constexpr auto make_enum_table<>(std::string_view, const enum_entry<E> (&)[N]) noexcept
      -> enum_table<E, N>;

    friend constexpr auto verify_enum_table<>(const enum_table<E, N> &) noexcept -> bool;

    [[nodiscard]] constexpr auto type_name() const noexcept -> std::string_view
    {
        return type_name_;
    }

    [[nodiscard]] constexpr auto entries() const noexcept -> const std::array<enum_entry<E>, N> &
    {
        return entries_;
    }

private:
    std::string_view type_name_;
    std::array<enum_entry<E>, N> entries_;
};

template <typename E, typename = void>
inline constexpr bool has_enum_table_v = false;

template <typename E>
inline constexpr bool
  has_enum_table_v<E, std::void_t<decltype(get_enum_table(static_cast<E *>(nullptr)))>> = true;

namespace detail {

// The bitwise union of all declared (non-zero) flag values
template <typename E>
constexpr auto known_mask() noexcept -> enum_underlying_t<E>
{
    using U = enum_underlying_t<E>;

    if constexpr (has_enum_table_v<E>) {
        constexpr auto table = get_enum_table(static_cast<E *>(nullptr));
        U mask{ 0 };
        for (const auto &entry : table.entries()) {
            mask |= static_cast<U>(entry.value);
        }

        return mask;
    } else {
        // A bitmask enum must register a table for its known bits to be
        // well-defined; otherwise truncate/all would silently no-op.
        static_assert(
          has_enum_table_v<E>,
          "bitmask enum requires a registered enum table (LINYAPS_REGISTER_ENUM_TABLE)");
    }
}

} // namespace detail

template <typename E, std::size_t N>
constexpr auto make_enum_table(std::string_view type_name,
                               const enum_entry<E> (&entries)[N]) noexcept -> enum_table<E, N>
{
    enum_table table(type_name, entries);

    // generate an ordered table at compile time
    if constexpr (is_bitmask_enum_v<E>) {
        using U = std::make_unsigned_t<detail::enum_underlying_t<E>>;

        detail::shell_sort(span(table.entries_),
                           [](const enum_entry<E> &a, const enum_entry<E> &b) {
                               const auto a_u = static_cast<U>(a.value);
                               const auto b_u = static_cast<U>(b.value);
                               const auto a_pop = detail::popcount(a_u);
                               const auto b_pop = detail::popcount(b_u);
                               return a_pop > b_pop || (a_pop == b_pop && a_u < b_u);
                           });
    }

    return table;
}

template <typename E, std::size_t N>
constexpr auto verify_enum_table(const enum_table<E, N> &table) noexcept -> bool
{
    // value duplicates: leverage existing sort order for bitmask enums
    if constexpr (is_bitmask_enum_v<E>) {
        for (std::size_t i = 1; i < N; ++i) {
            if (table.entries_[i].value == table.entries_[i - 1].value) {
                return false;
            }
        }
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t j = i + 1; j < N; ++j) {
                if (table.entries_[i].value == table.entries_[j].value) {
                    return false;
                }
            }
        }
    }

    // name duplicates: shell sort by name then check adjacent
    std::array<enum_entry<E>, N> sorted;
    for (std::size_t i = 0; i < N; ++i) {
        sorted[i] = table.entries_[i];
    }

    detail::shell_sort(span(sorted), [](const enum_entry<E> &a, const enum_entry<E> &b) {
        return a.name < b.name;
    });

    for (std::size_t i = 1; i < N; ++i) {
        if (sorted[i].name == sorted[i - 1].name) {
            return false;
        }
    }

    return true;
}

template <typename E, std::enable_if_t<is_bitmask_enum_v<E>, int> = 0>
class bitflags
{
public:
    using underlying_type = detail::enum_underlying_t<E>;

    static_assert(!std::is_signed_v<underlying_type>,
                  "bitflags<E> requires an unsigned underlying type");

    constexpr bitflags() noexcept = default;

    constexpr bitflags(E flag) noexcept
        : bits_(static_cast<underlying_type>(flag))
    {
    }

    [[nodiscard]] constexpr static auto from_raw(underlying_type bits) noexcept
      -> std::optional<bitflags>
    {
        if ((bits & ~detail::known_mask<E>()) != 0) {
            return std::nullopt;
        }

        return bitflags(bits);
    }

    // Drops unknown bits, keeping only the declared flags
    constexpr static auto from_raw_truncate(underlying_type bits) noexcept -> bitflags
    {
        return bitflags(bits & detail::known_mask<E>());
    }

    constexpr static auto all() noexcept -> bitflags { return bitflags(detail::known_mask<E>()); }

    [[nodiscard]] constexpr auto is_all() const noexcept -> bool
    {
        return bits_ == detail::known_mask<E>();
    }

    [[nodiscard]] constexpr auto to_raw() const noexcept -> underlying_type { return bits_; }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool { return bits_ == 0; }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return !empty(); }

    [[nodiscard]] constexpr auto contains(E flag) const noexcept -> bool
    {
        const auto f = static_cast<underlying_type>(flag);
        return (bits_ & f) == f;
    }

    [[nodiscard]] constexpr auto contains(bitflags flags) const noexcept -> bool
    {
        return (bits_ & flags.bits_) == flags.bits_;
    }

    [[nodiscard]] constexpr auto intersects(bitflags flags) const noexcept -> bool
    {
        return (bits_ & flags.bits_) != 0;
    }

    constexpr auto set(E flag, bool value = true) noexcept -> bitflags &
    {
        if (value) {
            bits_ |= static_cast<underlying_type>(flag);
        } else {
            bits_ &= ~static_cast<underlying_type>(flag);
        }

        return *this;
    }

    constexpr auto unset(E flag) noexcept -> bitflags & { return set(flag, false); }

    constexpr auto toggle(E flag) noexcept -> bitflags &
    {
        bits_ ^= static_cast<underlying_type>(flag);
        return *this;
    }

    constexpr auto clear() noexcept -> void { bits_ = 0; }

    constexpr auto operator|=(bitflags rhs) noexcept -> bitflags &
    {
        bits_ |= rhs.bits_;
        return *this;
    }

    constexpr auto operator&=(bitflags rhs) noexcept -> bitflags &
    {
        bits_ &= rhs.bits_;
        return *this;
    }

    constexpr auto operator^=(bitflags rhs) noexcept -> bitflags &
    {
        bits_ ^= rhs.bits_;
        return *this;
    }

    [[nodiscard]] friend constexpr auto operator|(bitflags lhs, bitflags rhs) noexcept -> bitflags
    {
        return bitflags{ static_cast<underlying_type>(lhs.bits_ | rhs.bits_) };
    }

    [[nodiscard]] friend constexpr auto operator&(bitflags lhs, bitflags rhs) noexcept -> bitflags
    {
        return bitflags{ static_cast<underlying_type>(lhs.bits_ & rhs.bits_) };
    }

    [[nodiscard]] friend constexpr auto operator^(bitflags lhs, bitflags rhs) noexcept -> bitflags
    {
        return bitflags{ static_cast<underlying_type>(lhs.bits_ ^ rhs.bits_) };
    }

    [[nodiscard]] friend constexpr auto operator~(bitflags rhs) noexcept -> bitflags
    {
        return bitflags{ static_cast<underlying_type>(~rhs.bits_)
                         & static_cast<underlying_type>(detail::known_mask<E>()) };
    }

    [[nodiscard]] friend constexpr auto operator==(bitflags lhs, bitflags rhs) noexcept -> bool
    {
        return lhs.bits_ == rhs.bits_;
    }

    [[nodiscard]] friend constexpr auto operator!=(bitflags lhs, bitflags rhs) noexcept -> bool
    {
        return lhs.bits_ != rhs.bits_;
    }

private:
    explicit constexpr bitflags(underlying_type bits) noexcept
        : bits_(bits)
    {
    }

    underlying_type bits_{ 0 };
};

template <typename E, std::enable_if_t<is_bitmask_enum_v<E>, int> = 0>
constexpr auto get_enum_table([[maybe_unused]] bitflags<E> *ptr) noexcept
{
    return get_enum_table(static_cast<E *>(nullptr));
}

} // namespace linyaps_box::utils

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
[[nodiscard]] constexpr auto operator|(E lhs, E rhs) noexcept -> linyaps_box::utils::bitflags<E>
{
    return linyaps_box::utils::bitflags<E>(lhs) | linyaps_box::utils::bitflags<E>(rhs);
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
[[nodiscard]] constexpr auto operator&(E lhs, E rhs) noexcept -> linyaps_box::utils::bitflags<E>
{
    return linyaps_box::utils::bitflags<E>(lhs) & linyaps_box::utils::bitflags<E>(rhs);
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
[[nodiscard]] constexpr auto operator^(E lhs, E rhs) noexcept -> linyaps_box::utils::bitflags<E>
{
    return linyaps_box::utils::bitflags<E>(lhs) ^ linyaps_box::utils::bitflags<E>(rhs);
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
[[nodiscard]] constexpr auto operator~(E rhs) noexcept -> linyaps_box::utils::bitflags<E>
{
    return ~linyaps_box::utils::bitflags<E>(rhs);
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
constexpr E &operator|=(E &lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::detail::enum_underlying_t<E>;
    return lhs = static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
constexpr E &operator&=(E &lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::detail::enum_underlying_t<E>;
    return lhs = static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E, std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>, int> = 0>
constexpr E &operator^=(E &lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::detail::enum_underlying_t<E>;
    return lhs = static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

#define LINYAPS_ENABLE_BITMASK_ENUM(E)                                           \
    [[maybe_unused]] constexpr ::std::true_type enable_bitmask_enum_tag(         \
      [[maybe_unused]] E *ptr) noexcept /* NOLINT(bugprone-macro-parentheses) */ \
    {                                                                            \
        return { };                                                              \
    }

#define LINYAPS_REGISTER_ENUM_TABLE(E, COUNT, ...)                                                \
    constexpr auto get_enum_table([[maybe_unused]] E *ptr) noexcept                               \
    {                                                                                             \
        constexpr auto table = ::linyaps_box::utils::make_enum_table<E>(#E, { __VA_ARGS__ });     \
        constexpr auto valid = ::linyaps_box::utils::verify_enum_table(table);                    \
        static_assert(valid,                                                                      \
                      "enum_table validation failed for " #E                                      \
                      ": duplicate name or value detected!");                                     \
        static_assert(table.entries().size() == static_cast<std::size_t>(COUNT),                  \
                      "enum_table entry count mismatch for " #E ": expected " #COUNT " entries"); \
        return table;                                                                             \
    }
