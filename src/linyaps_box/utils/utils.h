// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <fmt/format.h>
#include <zeus/expected.hpp>

#include <chrono>
#include <cstddef>
#include <ctime>
#include <random>
#include <string>
#include <tuple>

#include <unistd.h>

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

inline auto gen_random_string(std::size_t len) noexcept
{
    constexpr std::string_view charset = "0123456789"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz";

    thread_local std::mt19937 gen = []() noexcept -> std::mt19937 {
        try {
            return std::mt19937{ std::random_device{ }() };
        } catch (...) {
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            constexpr auto golden = 0x9e3779b97f4a7c15ULL;
            auto pid = static_cast<unsigned long>(getpid()) * golden;
            auto fallback_seed = static_cast<unsigned int>(now ^ pid);
            return std::mt19937{ fallback_seed };
        }
    }();
    std::uniform_int_distribution<std::size_t> dist(0, charset.size() - 1);

    std::string result;
    result.reserve(len);

    for (std::size_t i = 0; i < len; ++i) {
        result += charset[dist(gen)];
    }

    return result;
}

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
    void construct([[maybe_unused]] U *ptr, [[maybe_unused]] Args &&...args) noexcept
    {
        // 确保类型可平凡拷贝：std::vector 扩容 relocate 时会实例化此函数
        // （传入移动参数），但运行时对平凡可拷贝类型走 memmove 路径，
        // 空实现是安全的。此断言防止与非平凡类型误用导致 UB。
        static_assert(std::is_trivially_copyable_v<U>,
                      "uninit_allocator requires trivially copyable types");
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

} // namespace linyaps_box::utils
