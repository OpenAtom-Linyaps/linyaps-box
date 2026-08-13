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

// Enum traits are built around two opt-in mechanisms:
//
// 1. Bitmask support is an intrinsic property of the enum, so the marker is
//    written inside the enum body:
//
//        enum class some_flag : uint64_t {
//            ...
//            max = SOME_VALUE,
//            LINYAPS_MARK_AS_BITMASK_ENUM(max),   // sentinel enumerator
//        };
//
//    The macro injects a sentinel enumerator named
//    `LINYAPS_BITMASK_LARGEST_ENUMERATOR` holding the highest flag bit.
//    `is_bitmask_enum<E>` detects it by a plain member lookup, so no ADL free
//    function is needed for detection. The operators (`| & ^ ~` and the
//    compound forms) are defined once, at global scope, as templates
//    SFINAE-constrained on `is_bitmask_enum_v<E>`; ADL finds them for a marked
//    enum no matter which namespace it lives in, so they are usable anywhere in
//    the project without per-enum macro expansion.
//
//    The sentinel's value (the largest flag bit) bounds `operator~` to the
//    defined flag range via `bitmask_mask()`, so `~x` yields "all defined flags
//    except x" instead of a value with unrelated high bits set.
//
//    The macro argument must be an *enumerator*, not a literal: its value is
//    bound to the platform macro at the enum definition site, so it tracks the
//    platform automatically. The one invariant to uphold is that the declared
//    enumerator really is the maximum flag on every platform this code compiles
//    on. For table-registered enums `verify_enum_table()` enforces this at
//    compile time by requiring every entry's bits to fall within
//    `bitmask_mask()`; an enum without a table has no such check, so its
//    declared maximum must be verified by hand (all current ones are stable
//    across Linux architectures).
//
// 2. The name table is extrinsic metadata: entries are `{value, name}` pairs
//    that cannot be enumerators, so they cannot live inside the enum body. The
//    table is registered right after the enum instead:
//
//        LINYAPS_REGISTER_ENUM(some_flag, { some_flag::none, "NONE" }, ...);
//
//    which generates a `get_enum_table(E*)` free function in the enum's
//    namespace. `has_enum_table<E>` and the `fmt::formatter` specialization
//    below look it up via ADL, giving table-driven `to_name`/`from_name` and
//    automatic fmt formatting (splitting bitmask values into `A|B`).
//
// Limitation: `LINYAPS_MARK_AS_BITMASK_ENUM` injects a real enumerator. For an
// unscoped enum that enumerator leaks into the enclosing scope, so two
// unscoped bitmask enums in the same scope would collide on the sentinel name
// (scoped `enum class` enums are unaffected). A non-intrusive variant (explicit specialization) can
// be added later if that becomes a problem.

template <typename E>
using enum_underlying_t = std::underlying_type_t<E>;

template <typename E, typename = void>
struct is_bitmask_enum : std::false_type
{
};

template <typename E>
struct is_bitmask_enum<E, std::void_t<decltype(E::LINYAPS_BITMASK_LARGEST_ENUMERATOR)>>
    : std::true_type
{
};

template <typename E>
inline constexpr bool is_bitmask_enum_v = is_bitmask_enum<E>::value;

template <typename E, typename = void>
struct largest_bitmask_enum_bit
{
};

template <typename E>
struct largest_bitmask_enum_bit<E, std::void_t<decltype(E::LINYAPS_BITMASK_LARGEST_ENUMERATOR)>>
{
    using underlying_t = enum_underlying_t<E>;
    static constexpr underlying_t value =
      static_cast<underlying_t>(E::LINYAPS_BITMASK_LARGEST_ENUMERATOR);
};

// Smallest power of two strictly greater than `value`. On overflow (top bit
// set) the propagation fills the whole width and `+ 1` wraps to 0
template <typename T>
constexpr auto next_power_of_2(T value) noexcept -> T
{
    for (std::size_t shift = 1; shift < sizeof(T) * 8; shift <<= 1) {
        value |= static_cast<T>(value >> shift);
    }

    return static_cast<T>(value + 1);
}

template <typename E>
constexpr auto bitmask_mask() noexcept -> enum_underlying_t<E>
{
    using U = enum_underlying_t<E>;
    const auto largest = static_cast<U>(largest_bitmask_enum_bit<E>::value);
    // Every bit up to (but not including) the next power of two above the
    // largest flag: a contiguous 1-range covering every bit the enum can
    // express.
    return static_cast<U>(next_power_of_2(largest) - 1);
}

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

    if constexpr (is_bitmask_enum_v<E>) {
        using U = enum_underlying_t<E>;
        constexpr auto mask = bitmask_mask<E>();
        for (std::size_t i = 0; i < N; ++i) {
            const auto value = static_cast<U>(table.entries[i].value);
            if ((value & ~mask) != 0) {
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

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
[[nodiscard]] constexpr E operator|(E lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::enum_underlying_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
[[nodiscard]] constexpr E operator&(E lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::enum_underlying_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
[[nodiscard]] constexpr E operator^(E lhs, E rhs) noexcept
{
    using U = linyaps_box::utils::enum_underlying_t<E>;
    return static_cast<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
[[nodiscard]] constexpr E operator~(E rhs) noexcept
{
    using U = linyaps_box::utils::enum_underlying_t<E>;
    return static_cast<E>(~static_cast<U>(rhs) & linyaps_box::utils::bitmask_mask<E>());
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
constexpr E &operator|=(E &lhs, E rhs) noexcept
{
    return lhs = lhs | rhs;
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
constexpr E &operator&=(E &lhs, E rhs) noexcept
{
    return lhs = lhs & rhs;
}

template <typename E, typename = std::enable_if_t<linyaps_box::utils::is_bitmask_enum_v<E>>>
constexpr E &operator^=(E &lhs, E rhs) noexcept
{
    return lhs = lhs ^ rhs;
}

// Opts an enum into bitmask operators. Write it inside the enum body; it
// injects the sentinel enumerator LINYAPS_BITMASK_LARGEST_ENUMERATOR holding the
// highest flag bit, which bounds operator~ to the defined flag range.
//
// The argument must be the *enumerator* with the highest flag bit, NOT a
// literal: its value is bound to the platform macro at the enum definition site
// (e.g. `tmpfile = O_TMPFILE`), so it tracks the platform automatically. It must
// stay the true maximum on every platform this code compiles on; for
// table-registered enums verify_enum_table() enforces that at compile time.
#define LINYAPS_MARK_AS_BITMASK_ENUM(LargestValue) LINYAPS_BITMASK_LARGEST_ENUMERATOR = LargestValue

// GCC 8's constexpr evaluator may default-construct + copy the table
// when binding a reference inside a direct static_assert(verify(table)).
// Assigning to a constexpr variable first avoids that path.
#define LINYAPS_REGISTER_ENUM(E, ...)                                                     \
    constexpr auto get_enum_table(E *) noexcept                                           \
    {                                                                                     \
        constexpr auto table = ::linyaps_box::utils::make_enum_table<E>({ __VA_ARGS__ }); \
        constexpr auto valid = ::linyaps_box::utils::verify_enum_table(table);            \
        static_assert(valid,                                                              \
                      "enum_table validation failed for " #E                              \
                      ": duplicate name or value detected!");                             \
        return table;                                                                     \
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
