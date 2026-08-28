// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/time.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/chrono.h>

#include <cassert>
#include <charconv>
#include <cstdint>
#include <stdexcept>

namespace linyaps_box::utils {

auto to_created_time(span<char> buf,
                     std::chrono::system_clock::time_point tp,
                     subsecond_precision p) noexcept -> std::size_t
{
    assert(buf.size() >= max_created_time_len && "buffer too small for created time");

    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{ };
    ::gmtime_r(&t, &tm);

    auto *pos = fmt::format_to(buf.data(), "{:%Y-%m-%dT%H:%M:%S}", tm);

    std::uint64_t count{ 0 };
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
    ns -= (ns / 1'000'000'000) * 1'000'000'000;

    int digits{ 0 };
    switch (p) {
    case subsecond_precision::microseconds:
        count = static_cast<std::uint64_t>(ns) / 1000;
        digits = 6;
        break;
    case subsecond_precision::nanoseconds:
        count = static_cast<std::uint64_t>(ns);
        digits = 9;
        break;
    }

    pos = fmt::format_to(pos, ".{:0{}}Z", count, digits);

    const auto len = static_cast<std::size_t>(pos - buf.data());
    assert(len <= buf.size() && "created time overflowed buffer");
    return len;
}

auto from_created_time(const std::string &str) -> std::chrono::system_clock::time_point
{
    // Legacy 2.2.x status files stored "created" as nanoseconds since epoch.
    if (!str.empty() && str.find_first_not_of("0123456789") == std::string::npos) {
        std::uint64_t ns{ 0 };
        const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), ns);
        if (ec == std::errc{ } && ptr == str.data() + str.size()) {
            return std::chrono::system_clock::time_point{ std::chrono::nanoseconds{ ns } };
        }
    }

    std::tm tm{ };

    auto *end = ::strptime(str.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (UNLIKELY(end == nullptr)) {
        throw std::invalid_argument(fmt::format("failed to parse created time: {}", str));
    }

    std::chrono::nanoseconds fractional{ 0 };
    if (*end == '.') {
        ++end;

        std::array<char, 10> frac{ };
        auto frac_len = std::size_t{ 0 };
        while (*end >= '0' && *end <= '9' && frac_len < 9) {
            frac[frac_len++] = *end++;
        }

        while (frac_len < 9) {
            frac[frac_len++] = '0';
        }

        unsigned long ns{ 0 };
        auto [p, ec] = std::from_chars(frac.begin(), frac.end(), ns);
        if (UNLIKELY(ec != std::errc{ })) {
            throw std::invalid_argument(fmt::format("failed to parse fractional seconds: {}", str));
        }

        fractional = std::chrono::nanoseconds{ ns };
    }

    if (UNLIKELY(*end != 'Z' && *end != 'z')) {
        throw std::invalid_argument(fmt::format("expected 'Z' timezone in created time: {}", str));
    }

    if (UNLIKELY(*++end != '\0')) {
        throw std::invalid_argument(
          fmt::format("unexpected trailing characters in created time: {}", str));
    }

    auto tp = std::chrono::system_clock::from_time_t(timegm(&tm));
    tp += fractional;
    return tp;
}

} // namespace linyaps_box::utils
