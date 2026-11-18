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
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_FIFO", "nice": 5}})"),
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

TEST(SchedulerParse, NiceAcceptedForBatchPolicy)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_BATCH", "nice": 5}})");
    ASSERT_TRUE(config.process_->scheduler_.has_value());
    EXPECT_THAT(config.process_->scheduler_->nice.value_or(0), Eq(5));
}

TEST(SchedulerParse, NiceBelowRangeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_BATCH", "nice": -30}})"),
      std::runtime_error);
}

TEST(SchedulerParse, ZeroPriorityAcceptedForOtherPolicy)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "priority": 0}})");
    ASSERT_TRUE(config.process_->scheduler_.has_value());
    EXPECT_THAT(config.process_->scheduler_->priority.value_or(1), Eq(0));
}

TEST(SchedulerParse, DeadlineParametersAcceptedForDeadlinePolicy)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_DEADLINE", "runtime": 100000, "deadline": 200000, "period": 300000}})");
    ASSERT_TRUE(config.process_->scheduler_.has_value());
    ASSERT_TRUE(config.process_->scheduler_->runtime.has_value());
    EXPECT_THAT(config.process_->scheduler_->runtime.value_or(0), Eq(100000U));
}

TEST(SchedulerParse, RealtimePriorityAccepted)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_FIFO", "priority": 10}})");
    ASSERT_TRUE(config.process_->scheduler_.has_value());
    EXPECT_THAT(config.process_->scheduler_->priority.value_or(0), Eq(10));
}

TEST(SchedulerParse, PeriodWithNonDeadlinePolicyRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "scheduler": {"policy": "SCHED_OTHER", "period": 1000}})"),
      std::runtime_error);
}

} // namespace
} // namespace linyaps_box
