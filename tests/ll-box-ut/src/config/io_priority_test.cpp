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

TEST(IoPriorityParse, UnknownClassRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "ioPriority": {"class": "IOPRIO_CLASS_NOPE", "priority": 5}})"),
      std::runtime_error);
}

TEST(IoPriorityParse, PriorityOutOfRangeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "ioPriority": {"class": "IOPRIO_CLASS_RT", "priority": 8}})"),
      std::runtime_error);
}

TEST(IoPriorityParse, HappyPath)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "ioPriority": {"class": "IOPRIO_CLASS_BE", "priority": 4}})");
    ASSERT_TRUE(config.process_.has_value());
    ASSERT_TRUE(config.process_->io_priority_.has_value());
    EXPECT_EQ(config.process_->io_priority_->class_, io_priority::class_t::best_effort);
    EXPECT_EQ(config.process_->io_priority_->priority, 4);
}

} // namespace
} // namespace linyaps_box
