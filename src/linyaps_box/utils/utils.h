// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <tuple>

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

namespace linyaps_box::utils {

template<typename T>
struct type_entity
{
    using type = T;
};

template<std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args...))
{
    static_assert(N >= 1 && N <= sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N - 1, std::tuple<Args...>>>{};
}

template<std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args..., ...))
{
    static_assert(N >= 1 && N <= sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N - 1, std::tuple<Args...>>>{};
}

template<typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template<typename... T>
Overload(T...) -> Overload<T...>;

} // namespace linyaps_box::utils
