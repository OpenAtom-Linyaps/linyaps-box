// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/utils/date.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace utils = linyaps_box::utils;

namespace {

auto make_time_point(std::int64_t ns) -> std::chrono::system_clock::time_point
{
    return std::chrono::system_clock::time_point{ std::chrono::nanoseconds{ ns } };
}

auto format(std::chrono::system_clock::time_point tp,
            utils::subsecond_precision p = utils::subsecond_precision::microseconds) -> std::string
{
    std::array<char, utils::max_created_time_len> buf{ };
    auto len = utils::to_created_time(utils::span{ buf }, tp, p);
    return std::string{ buf.data(), len };
}

} // namespace

TEST(ToCreatedTime, EpochMicroseconds)
{
    EXPECT_EQ(format(make_time_point(0)), "1970-01-01T00:00:00.000000Z");
}

TEST(ToCreatedTime, NegativeEpoch)
{
    EXPECT_EQ(format(make_time_point(-1'000'000'000)), "1969-12-31T23:59:59.000000Z");
}

TEST(ToCreatedTime, NanosecondsPrecision)
{
    EXPECT_EQ(
      format(make_time_point(1'700'000'000'123'456'789), utils::subsecond_precision::nanoseconds),
      "2023-11-14T22:13:20.123456789Z");
}

TEST(FromCreatedTime, RoundTripMicroseconds)
{
    auto tp = make_time_point(1'700'000'000'123'456'000);
    EXPECT_EQ(utils::from_created_time(format(tp)), tp);
}

TEST(FromCreatedTime, RoundTripNanoseconds)
{
    auto tp = make_time_point(1'700'000'000'123'456'789);
    EXPECT_EQ(utils::from_created_time(format(tp, utils::subsecond_precision::nanoseconds)), tp);
}

TEST(FromCreatedTime, LegacyPureNanos)
{
    EXPECT_EQ(utils::from_created_time("1700000000123456000"),
              make_time_point(1'700'000'000'123'456'000));
}

TEST(FromCreatedTime, EmptyThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time(""), std::invalid_argument);
}

TEST(FromCreatedTime, OutOfRangeComponentsThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-13-01T00:00:00Z"),
                 std::invalid_argument);
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-32T00:00:00Z"),
                 std::invalid_argument);
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-01T24:00:00Z"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, MissingTimezoneThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-01T00:00:00"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, TrailingCharactersThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-01T00:00:00Zx"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, FixedFieldTrailingNonDigitThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-0aT00:00:00Z"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, InvalidPureNanosThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("123abc"), std::invalid_argument);
}

TEST(FromCreatedTime, PureNanosOutOfRangeThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("9223372036854775808"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, YearOutOfRangeThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("9999-12-31T23:59:59Z"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, EmptyFractionThrows)
{
    EXPECT_THROW(std::ignore = utils::from_created_time("2026-01-01T00:00:00.Z"),
                 std::invalid_argument);
}

TEST(FromCreatedTime, MaxRepresentableRoundTrip)
{
    auto ns = std::chrono::nanoseconds{ std::numeric_limits<std::int64_t>::max() };
    auto tp = std::chrono::system_clock::time_point{ ns };
    auto str = format(tp, utils::subsecond_precision::nanoseconds);
    EXPECT_EQ(utils::from_created_time(str), tp);
}
