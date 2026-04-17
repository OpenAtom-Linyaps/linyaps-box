// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
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

template<typename Entity>
using extract_t = typename Entity::type;

template<std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args...))
{
    static_assert(N < sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N, std::tuple<Args...>>>{ };
}

template<std::size_t N, typename R, typename... Args>
constexpr auto get_n_params_type([[maybe_unused]] R (*ptr)(Args..., ...))
{
    static_assert(N < sizeof...(Args), "index out of range");
    return type_entity<std::tuple_element_t<N, std::tuple<Args...>>>{ };
}

template<typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template<typename... T>
Overload(T...) -> Overload<T...>;

template<typename T>
std::string stringify_arg(T arg)
{
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        if (arg == nullptr) {
            return "nullptr";
        }

        return std::string(arg);
    } else if constexpr (std::is_pointer_v<T>) {
        if (arg == nullptr) {
            return "nullptr";
        }

        std::array<char, 20> buf{ };
        auto [ptr, ec] =
                std::to_chars(buf.begin(), buf.end(), reinterpret_cast<uintptr_t>(arg), 16);
        return "0x" + std::string(buf.begin(), ptr);
    } else {
        return std::to_string(arg);
    }
}

} // namespace linyaps_box::utils
