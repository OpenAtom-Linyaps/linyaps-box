// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "fixture.h"

namespace linyaps_box {

namespace {

using linyaps_box::test::parse_config;

TEST(TimeOffsetParse, NanosecsBoundAccepted)
{
    const auto config = parse_config(
      R"(  "linux": {"timeOffsets": {"boottime": {"secs": 1, "nanosecs": 999999999}}})");
    ASSERT_TRUE(config.linux_->time_offsets.has_value());
    ASSERT_TRUE(config.linux_->time_offsets->at("boottime").nanosecs.has_value());
    EXPECT_EQ(*config.linux_->time_offsets->at("boottime").nanosecs, 999999999U);
}

TEST(TimeOffsetParse, NanosecsBoundRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"timeOffsets": {"boottime": {"secs": 1, "nanosecs": 1000000000}}})"),
      std::runtime_error);
}

TEST(TimeOffsetParse, NegativeSecsAccepted)
{
    const auto config =
      parse_config(R"(  "linux": {"timeOffsets": {"boottime": {"secs": -5, "nanosecs": 1}}})");
    ASSERT_TRUE(config.linux_->time_offsets.has_value());
    ASSERT_TRUE(config.linux_->time_offsets->at("boottime").secs.has_value());
    EXPECT_EQ(*config.linux_->time_offsets->at("boottime").secs, -5);
}

} // namespace
} // namespace linyaps_box
