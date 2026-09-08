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

TEST(NetworkParse, EmptyPriorityNameRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"network": {"priorities": [{"name": "", "priority": 5}]}}})"),
      std::runtime_error);
}

TEST(NetworkParse, HappyPath)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"network": {"classID": 1048577,
                                      "priorities": [{"name": "eth0", "priority": 5}]}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->network_.has_value());
    EXPECT_EQ(config.linux_->resources_->network_->class_id, 1048577U);
    ASSERT_TRUE(config.linux_->resources_->network_->priorities.has_value());
    EXPECT_EQ(config.linux_->resources_->network_->priorities->at(0).name, "eth0");
}

TEST(NetworkParse, NullFieldsAccepted)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"network": null}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    EXPECT_FALSE(config.linux_->resources_->network_.has_value());
}

} // namespace
} // namespace linyaps_box
