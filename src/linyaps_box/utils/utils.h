// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/format.h>
#include <zeus/expected.hpp>

#include <cstddef>
#include <string>
#include <tuple>
#if defined(__GNUC__) || defined(__clang__)
#  define LIKELY(x) __builtin_expect((x), 1)
#  define UNLIKELY(x) __builtin_expect((x), 0)
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

namespace linyaps_box::utils {

template <typename T>
struct type_entity
{
    using type = T;
};

template <typename Entity>
using extract_t = typename Entity::type;

template <std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args...))
{
    static_assert(N < sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N, std::tuple<Args...>>>{ };
}

template <std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args..., ...))
{
    static_assert(N < sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N, std::tuple<Args...>>>{ };
}

template <typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template <typename... T>
Overload(T...) -> Overload<T...>;

template <typename Alloc, typename T>
void append_arg(std::basic_string<char, std::char_traits<char>, Alloc> &msg, bool &first, T &&arg)
{
    using Decayed = std::decay_t<T>;

    if (!first) {
        fmt::format_to(std::back_inserter(msg), ", ");
    }
    first = false;

    if constexpr (std::is_pointer_v<Decayed> || std::is_null_pointer_v<Decayed>) {
        if (arg == nullptr) {
            fmt::format_to(std::back_inserter(msg), "nullptr");
        } else if constexpr (std::is_convertible_v<Decayed, std::string_view>) {
            fmt::format_to(std::back_inserter(msg), "{}", arg);
        } else {
            fmt::format_to(std::back_inserter(msg), "{}", static_cast<const void *>(arg));
        }
    } else {
        fmt::format_to(std::back_inserter(msg), "{}", std::forward<T>(arg));
    }
}

} // namespace linyaps_box::utils
