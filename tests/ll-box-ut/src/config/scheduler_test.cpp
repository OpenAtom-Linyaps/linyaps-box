// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::Eq;

using linyaps_box::test::parse_config;

TEST(SchedulerParse, UnknownPolicyRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_NOPE"}})"),
      std::runtime_error);
}

TEST(SchedulerParse, UnknownFlagRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "flags": ["SCHED_FLAG_NOPE"]}})"),
      std::runtime_error);
}

TEST(SchedulerParse, NiceOutOfRangeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "nice": 30}})"),
      std::runtime_error);
}

TEST(SchedulerParse, NiceWithRealTimePolicyRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_FIFO", "nice": 30}})"),
      std::runtime_error);
}

TEST(SchedulerParse, PriorityWrongPolicyRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "priority": 5}})"),
      std::runtime_error);
}

TEST(SchedulerParse, DeadlineFieldsWrongPolicyRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "runtime": 100000}})"),
      std::runtime_error);
}

TEST(SchedulerParse, DeadlineHappyPath)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_DEADLINE", "runtime": 100000, "deadline": 200000, "period": 300000}})");
    ASSERT_TRUE(config.process_.has_value());
    ASSERT_TRUE(config.process_->scheduler_.has_value());
    EXPECT_EQ(config.process_->scheduler_->policy_, scheduler::policy::deadline);
    EXPECT_THAT(config.process_->scheduler_->runtime, Eq(100000));
}

TEST(SchedulerParse, PolicyRequiredRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"nice": 0}})"),
      std::runtime_error);
}

} // namespace
} // namespace linyaps_box
