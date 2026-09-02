// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/span.h"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace linyaps_box::utils {

enum class subsecond_precision : std::uint8_t {
    microseconds,
    nanoseconds,
};

// "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" — 30 chars is maximum (nanosecond precision).
constexpr auto max_created_time_len = std::size_t{ 30 };
[[nodiscard]] auto
to_created_time(span<char, max_created_time_len> buf,
                std::chrono::system_clock::time_point tp,
                subsecond_precision p = subsecond_precision::microseconds) noexcept -> std::size_t;

auto from_created_time(std::string_view str) -> std::chrono::system_clock::time_point;

} // namespace linyaps_box::utils
