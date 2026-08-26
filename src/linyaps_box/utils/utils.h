// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <chrono>
#include <random>
#include <string>

#include <unistd.h>

#if defined(__GNUC__) || defined(__clang__)
#  define LIKELY(x) __builtin_expect((x), 1)
#  define UNLIKELY(x) __builtin_expect((x), 0)
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

namespace linyaps_box::utils {

template <typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template <typename... T>
Overload(T...) -> Overload<T...>;

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

} // namespace linyaps_box::utils
