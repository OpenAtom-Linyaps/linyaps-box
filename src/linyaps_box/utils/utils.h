// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#  define LIKELY(x) __builtin_expect((x), 1)
#  define UNLIKELY(x) __builtin_expect((x), 0)
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

namespace linyaps_box::utils {

namespace detail {

template <typename T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(T (&arr)[N], std::index_sequence<I...>)
{
    return { { arr[I]... } };
}

template <typename T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(T (&&arr)[N], std::index_sequence<I...>)
{
    return { { std::move(arr[I])... } };
}

template <auto... Values>
struct shrink_macros
{
private:
    static constexpr bool has_negative = ((Values < 0) || ...);

    static constexpr bool has_overflow_int64 =
      ((Values > std::numeric_limits<std::int64_t>::max()) || ...);

    static_assert(!(has_negative && has_overflow_int64),
                  "Cannot fit both negative values and values exceeding INT64_MAX into standard "
                  "64-bit integer types.");

    static constexpr auto deduce_type()
    {
        if constexpr (!has_negative) {
            constexpr std::uint64_t max_val = std::max({ static_cast<std::uint64_t>(Values)... });

            if constexpr (max_val <= std::numeric_limits<std::uint8_t>::max()) {
                return std::uint8_t{ };
            } else if constexpr (max_val <= std::numeric_limits<std::uint16_t>::max()) {
                return std::uint16_t{ };
            } else if constexpr (max_val <= std::numeric_limits<std::uint32_t>::max()) {
                return std::uint32_t{ };
            } else {
                return std::uint64_t{ };
            }
        } else {
            constexpr std::int64_t min_val = std::min({ static_cast<std::int64_t>(Values)... });
            constexpr std::int64_t max_val = std::max({ static_cast<std::int64_t>(Values)... });

            if constexpr (min_val >= std::numeric_limits<std::int8_t>::min()
                          && max_val <= std::numeric_limits<std::int8_t>::max()) {
                return std::int8_t{ };
            } else if constexpr (min_val >= std::numeric_limits<std::int16_t>::min()
                                 && max_val <= std::numeric_limits<std::int16_t>::max()) {
                return std::int16_t{ };
            } else if constexpr (min_val >= std::numeric_limits<std::int32_t>::min()
                                 && max_val <= std::numeric_limits<std::int32_t>::max()) {
                return std::int32_t{ };
            } else {
                return std::int64_t{ };
            }
        }
    }

public:
    using type = decltype(deduce_type());
};

} // namespace detail

template <typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template <typename... T>
Overload(T...) -> Overload<T...>;

auto gen_random_string(std::size_t len) noexcept -> std::string;

template <typename T>
struct uninit_allocator
{
    static_assert(std::is_trivially_default_constructible_v<T>,
                  "uinit_allocator can only be used with trivially default constructible types!");
    using value_type = T;

    uninit_allocator() noexcept = default;

    T *allocate(std::size_t n) { return static_cast<T *>(::operator new(n * sizeof(T))); }

    void deallocate(T *p, [[maybe_unused]] std::size_t size) noexcept { ::operator delete(p); }

    template <typename U, typename... Args>
    void construct(U *ptr, Args &&...args) noexcept
    {
        static_assert(std::is_trivially_copyable_v<U>,
                      "uninit_allocator requires trivially copyable types");

        if constexpr (sizeof...(Args) > 0) {
            ::new (static_cast<void *>(ptr)) U(std::forward<Args>(args)...);
        }
    }

    template <typename U>
    void destroy([[maybe_unused]] U *ptr) noexcept
    {
    }

    bool operator==([[maybe_unused]] const uninit_allocator &other) const noexcept { return true; }

    bool operator!=([[maybe_unused]] const uninit_allocator &other) const noexcept { return false; }
};

template <typename T>
using uninit_vector = std::vector<T, uninit_allocator<T>>;

// c++ 20 to_array
template <typename T, std::size_t N>
constexpr auto to_array(T (&arr)[N]) noexcept
{
    return detail::to_array_impl(arr, std::make_index_sequence<N>{ });
}

template <typename T, std::size_t N>
constexpr auto to_array(T (&&arr)[N]) noexcept
{
    return detail::to_array_impl(std::move(arr), std::make_index_sequence<N>{ });
}

template <auto... Values>
using shrink_macros_t = typename detail::shrink_macros<Values...>::type;

} // namespace linyaps_box::utils
