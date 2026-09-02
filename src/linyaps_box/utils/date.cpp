// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/date.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace linyaps_box::utils {

namespace detail {

// from Howard Hinnant
// ref: https://howardhinnant.github.io/date_algorithms.html#days_from_civil
constexpr auto days_from_civil(std::int64_t y, unsigned m, unsigned d) noexcept -> std::int64_t
{
    static_assert(std::numeric_limits<unsigned>::digits >= 18,
                  "This algorithm has not been ported to a 16 bit unsigned integer");

    y -= ((m <= 2) ? 1 : 0);
    const auto era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - (era * 400));
    const unsigned doy = (((153 * (m > 2 ? m - 3 : m + 9)) + 2) / 5) + d - 1;
    const unsigned doe = (yoe * 365) + (yoe / 4) - (yoe / 100) + doy;
    return (era * 146097) + static_cast<std::int64_t>(doe) - 719468;
}

constexpr auto civil_from_days(std::int64_t z) noexcept
  -> std::tuple<std::int64_t, unsigned, unsigned>
{
    static_assert(std::numeric_limits<unsigned>::digits >= 18,
                  "This algorithm has not been ported to a 16 bit unsigned integer");

    z += 719468;
    const auto era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned>(z - (era * 146097));                       // [0, 146096]
    const unsigned yoe = (doe - (doe / 1460) + (doe / 36524) - (doe / 146096)) / 365; // [0, 399]
    const auto y = static_cast<std::int64_t>(yoe) + (era * 400);
    const unsigned doy = doe - ((365 * yoe) + (yoe / 4) - (yoe / 100)); // [0, 365]
    const unsigned mp = ((5 * doy) + 2) / 153;                          // [0, 11]
    const unsigned d = doy - (((153 * mp) + 2) / 5) + 1;                // [1, 31]
    const unsigned m = mp < 10 ? mp + 3 : mp - 9;                       // [1, 12]

    return { y + static_cast<std::int64_t>(m <= 2), m, d };
}

// ISO 8601: "YYYY-MM-DDTHH:MM:SS[.fffffffff]Z"
inline auto parse_iso8601(std::string_view str) -> std::chrono::nanoseconds
{
    if (UNLIKELY(str.size() < 19 || str[4] != '-' || str[7] != '-' || str[10] != 'T'
                 || str[13] != ':' || str[16] != ':')) {
        throw std::invalid_argument("invalid iso8601 format");
    }

    auto parse_integer =
      [](std::string_view sv, std::size_t offset, std::size_t len) -> std::uint64_t {
        std::uint64_t val{ 0 };

        auto [ptr, ec] = std::from_chars(sv.data() + offset, sv.data() + offset + len, val);
        if (UNLIKELY(ec != std::errc{ } || ptr != sv.data() + offset + len)) {
            throw std::invalid_argument("number parsing failed");
        }

        return val;
    };

    const auto year = parse_integer(str, 0, 4);
    const auto month = parse_integer(str, 5, 2);
    const auto day = parse_integer(str, 8, 2);
    const auto hour = parse_integer(str, 11, 2);
    const auto min = parse_integer(str, 14, 2);
    const auto sec = parse_integer(str, 17, 2);

    if (UNLIKELY(month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59
                 || sec > 60)) {
        throw std::invalid_argument("date time component out of range");
    }

    const auto days_epoch = detail::days_from_civil(static_cast<std::int64_t>(year),
                                                    static_cast<unsigned>(month),
                                                    static_cast<unsigned>(day));

    // from c++ 20
    using days = std::chrono::duration<std::int64_t, std::ratio<86400>>;

    // int64 nanoseconds can represent at most ~106751 days from the epoch.
    // Bound the day component so converting it to nanoseconds cannot overflow.
    static constexpr std::int64_t max_epoch_days =
      std::numeric_limits<std::int64_t>::max() / 86'400'000'000'000;
    if (UNLIKELY(days_epoch < -max_epoch_days || days_epoch > max_epoch_days)) {
        throw std::invalid_argument("date time out of representable range");
    }

    std::size_t pos{ 19 };
    std::uint64_t ns_val{ 0 };

    if (pos < str.size() && str[pos] == '.') {
        ++pos;

        std::size_t digits{ 0 };

        while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9') {
            if (digits < 9) {
                ns_val = (ns_val * 10) + static_cast<std::uint64_t>(str[pos] - '0');
                ++digits;
            }

            ++pos;
        }

        if (UNLIKELY(digits == 0)) {
            throw std::invalid_argument("fractional part must contain at least one digit");
        }

        static constexpr std::array<std::uint64_t, 10> pow10 = {
            1000000000ULL, 100000000ULL, 10000000ULL, 1000000ULL, 100000ULL,
            10000ULL,      1000ULL,      100ULL,      10ULL,      1ULL
        };

        ns_val *= pow10[digits];
    }

    if (UNLIKELY(pos >= str.size() || (str[pos] != 'Z' && str[pos] != 'z'))) {
        throw std::invalid_argument("expected 'Z' timezone");
    }

    if (UNLIKELY(++pos != str.size())) {
        throw std::invalid_argument("unexpected trailing characters");
    }

    const auto day_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(days{ days_epoch }).count();
    const auto tod_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::hours{ hour } + std::chrono::minutes{ min } + std::chrono::seconds{ sec })
        .count()
      + static_cast<std::int64_t>(ns_val);

    // day_ns near the boundary plus a full time-of-day would overflow int64.
    if (UNLIKELY(day_ns > 0 && tod_ns > std::numeric_limits<std::int64_t>::max() - day_ns)) {
        throw std::invalid_argument("date time out of representable range");
    }

    return std::chrono::nanoseconds{ day_ns + tod_ns };
}

auto parse_pure_nanos(std::string_view str) -> std::chrono::nanoseconds
{
    std::uint64_t nanos = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), nanos);

    if (UNLIKELY(ec != std::errc{ } || ptr != str.data() + str.size())) {
        throw std::invalid_argument("invalid integer nanoseconds format");
    }

    if (UNLIKELY(nanos > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) {
        throw std::invalid_argument("nanoseconds value out of range");
    }

    return std::chrono::nanoseconds{ static_cast<std::int64_t>(nanos) };
}

} // namespace detail

auto to_created_time(span<char, max_created_time_len> buf,
                     std::chrono::system_clock::time_point tp,
                     subsecond_precision p) noexcept -> std::size_t
{
    const auto total_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();

    auto total_sec = total_ns / 1'000'000'000;
    auto ns = static_cast<std::int32_t>(total_ns % 1'000'000'000);
    if (ns < 0) {
        ns += 1'000'000'000;
        total_sec -= 1;
    }

    auto days_epoch = total_sec / 86400;
    auto sec_in_day = static_cast<std::int32_t>(total_sec % 86400);
    if (sec_in_day < 0) {
        sec_in_day += 86400;
        days_epoch -= 1;
    }

    const auto [year, month, day] = detail::civil_from_days(days_epoch);

    const auto hour = static_cast<std::uint32_t>(sec_in_day / 3600);
    const auto rem = static_cast<std::uint32_t>(sec_in_day % 3600);
    const auto min = rem / 60;
    const auto sec = rem % 60;

    static constexpr std::array<char, 201> digits_lut{ "0001020304050607080910111213141516171819"
                                                       "2021222324252627282930313233343536373839"
                                                       "4041424344454647484950515253545556575859"
                                                       "6061626364656667686970717273747576777879"
                                                       "8081828384858687888990919293949596979899" };

    auto write_2dig = [](char *ptr, std::uint64_t val) -> char * {
        const auto idx = static_cast<std::size_t>(val * 2);
        ptr[0] = digits_lut[idx];
        ptr[1] = digits_lut[idx + 1];
        return ptr + 2;
    };

    auto write_4dig = [write_2dig](char *ptr, std::uint64_t val) -> char * {
        ptr = write_2dig(ptr, val / 100);
        return write_2dig(ptr, val % 100);
    };

    auto *pos = buf.data();
    pos = write_4dig(pos, year);
    *pos++ = '-';
    pos = write_2dig(pos, month);
    *pos++ = '-';
    pos = write_2dig(pos, day);
    *pos++ = 'T';
    pos = write_2dig(pos, hour);
    *pos++ = ':';
    pos = write_2dig(pos, min);
    *pos++ = ':';
    pos = write_2dig(pos, sec);
    *pos++ = '.';

    if (p == subsecond_precision::microseconds) {
        const auto us_val = static_cast<std::uint32_t>(ns / 1000); // 0..999999
        pos = write_2dig(pos, us_val / 10000);
        pos = write_2dig(pos, (us_val / 100) % 100);
        pos = write_2dig(pos, us_val % 100);
    } else {
        const auto ns_val = static_cast<std::uint32_t>(ns); // 0..999999999
        *pos++ = static_cast<char>('0' + (ns_val / 100000000));
        pos = write_2dig(pos, (ns_val / 1000000) % 100);
        pos = write_2dig(pos, (ns_val / 10000) % 100);
        pos = write_2dig(pos, (ns_val / 100) % 100);
        pos = write_2dig(pos, ns_val % 100);
    }

    *pos++ = 'Z';

    const auto len = static_cast<std::size_t>(pos - buf.data());
    return len;
}

auto from_created_time(std::string_view str) -> std::chrono::system_clock::time_point
{
    try {
        std::chrono::nanoseconds total_ns;

        if (str.find('T') != std::string_view::npos || str.find('-') != std::string_view::npos) {
            total_ns = detail::parse_iso8601(str); // after linyaps-box 2.3.0, use ISO 8601 format
        } else {
            total_ns = detail::parse_pure_nanos(str); // legacy format: pure nanoseconds since epoch
        }

        using namespace std::chrono;
        return system_clock::time_point{ duration_cast<system_clock::duration>(total_ns) };
    } catch (const std::exception &e) {
        throw std::invalid_argument(
          fmt::format("failed to parse created time '{}': {}", str, e.what()));
    }
}

} // namespace linyaps_box::utils
