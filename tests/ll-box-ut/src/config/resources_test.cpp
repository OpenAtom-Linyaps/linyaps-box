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

using testing::ElementsAre;
using testing::Eq;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

TEST(ResourcesParse, FixtureResources)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.linux_.has_value());
    const auto &linux = *config.linux_;
    ASSERT_TRUE(linux.resources_.has_value());
    const auto &resources = *linux.resources_;
    ASSERT_TRUE(resources.cpu_.has_value());
    EXPECT_THAT(resources.cpu_->shares, Eq(512));
    ASSERT_TRUE(resources.cpu_->cpus.has_value());
    EXPECT_THAT(*resources.cpu_->cpus, ElementsAre(0, 1, 2, 3));
    EXPECT_EQ(resources.cpu_->idle_, cpu::idle::idle);
    ASSERT_TRUE(resources.memory_.has_value());
    EXPECT_THAT(resources.memory_->limit, Eq(1048576));
}

TEST(ResourcesParse, BlockIOWeightDeviceRequiresWeight)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"blockIO": {"weightDevice": [{"major": 8, "minor": 0}]}}})"),
      std::runtime_error);
}

TEST(ResourcesParse, InvalidDeviceRuleTypeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"devices": [{"allow": true, "type": "x", "access": "rwm"}]}})"),
      std::runtime_error);
}

TEST(ResourcesParse, InvalidDeviceRuleAccessRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"devices": [{"allow": true, "type": "c", "access": "rwx"}]}})"),
      std::runtime_error);
}

TEST(ResourcesParse, EmptyHugepagePageSizeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "", "limit": 1024}]}})"),
      std::runtime_error);
}

TEST(ResourcesParse, SwappinessOutOfRangeRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"resources": {"memory": {"swappiness": 101}}})"),
                 std::runtime_error);
}

TEST(ResourcesParse, DeviceRuleAllowRequiredRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"resources": {"devices": [{"type": "c", "access": "rwm"}]}})"),
                 std::runtime_error);
}

TEST(ResourcesParse, EmptyBlockIOValidates)
{
    EXPECT_NO_THROW(std::ignore = parse_config(R"(  "linux": {"resources": {"blockIO": {}}})"));
}

TEST(ResourcesParse, EmptyNetworkValidates)
{
    EXPECT_NO_THROW(std::ignore = parse_config(R"(  "linux": {"resources": {"network": {}}})"));
}

TEST(ResourcesParse, RdmaRequiresHandlesOrObjects)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"resources": {"rdma": {"mlx5_1": {}}}})"),
                 std::runtime_error);
}

TEST(ResourcesParse, RdmaHappyPath)
{
    const auto config =
      parse_config(R"(  "linux": {"resources": {"rdma": {"mlx5_1": {"hcaObjects": 1000}}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->rdma_.has_value());
    const auto &entry = config.linux_->resources_->rdma_->at("mlx5_1");
    EXPECT_EQ(entry.hca_objects, 1000U);
    EXPECT_FALSE(entry.hca_handles.has_value());
}

TEST(ResourcesParse, HugepagePageSizeInvalidFormatRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "2MBx", "limit": 1024}]}})"),
      std::runtime_error);
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "abc", "limit": 1024}]}})"),
      std::runtime_error);
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "64K", "limit": 1024}]}})"),
      std::runtime_error);
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "1048576", "limit": 1024}]}})"),
      std::runtime_error);
}

TEST(ResourcesParse, HugepagePageSizeValidFormatsAccepted)
{
    const auto config = parse_config(
      R"(  "linux": {"resources": {"hugepageLimits": [
             {"pageSize": "2MB", "limit": 209715200},
             {"pageSize": "64KB", "limit": 1024},
             {"pageSize": "1048576B", "limit": 1024}
           ]}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->hugepage_limits.has_value());
    EXPECT_THAT(config.linux_->resources_->hugepage_limits->size(), 3U);
}

} // namespace
} // namespace linyaps_box
