// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/utils.h"

#include <chrono>
#include <random>
#include <thread>

#include <unistd.h>

namespace linyaps_box::utils {
auto gen_random_string(std::size_t len) noexcept -> std::string
{
    constexpr std::string_view charset = "0123456789"
                                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz";

    thread_local std::mt19937 gen = []() noexcept -> std::mt19937 {
        try {
            return std::mt19937{ std::random_device{ }() };
        } catch (...) {
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            const auto pid = static_cast<std::uint64_t>(::getpid());
            const auto tid = std::hash<std::thread::id>{ }(std::this_thread::get_id());

            const auto addr = reinterpret_cast<std::uintptr_t>(&now);

            std::seed_seq seq{
                static_cast<std::uint32_t>(now),       static_cast<std::uint32_t>(now >> 32),
                static_cast<std::uint32_t>(pid),       static_cast<std::uint32_t>(tid),
                static_cast<std::uint32_t>(tid >> 32), static_cast<std::uint32_t>(addr),
                static_cast<std::uint32_t>(addr >> 32)
            };

            return std::mt19937{ seq };
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
} // namespace linyaps_box::utils
