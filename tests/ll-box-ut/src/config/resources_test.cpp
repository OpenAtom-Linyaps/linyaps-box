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
using testing::SizeIs;

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
    EXPECT_EQ(*resources.cpu_->cpus, "0-3");
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

// An empty object yields a present-but-empty resource (distinct from the
// "null" case, which leaves the optional unset).
TEST(ResourcesParse, EmptyBlockIoParsedAsPresentButEmpty)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"blockIO": {}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->block_io_.has_value());
    EXPECT_FALSE(config.linux_->resources_->block_io_->weight.has_value());
    EXPECT_FALSE(config.linux_->resources_->block_io_->weight_devices.has_value());
}

TEST(ResourcesParse, EmptyNetworkParsedAsPresentButEmpty)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"network": {}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->network_.has_value());
    EXPECT_FALSE(config.linux_->resources_->network_->class_id.has_value());
    EXPECT_FALSE(config.linux_->resources_->network_->priorities.has_value());
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
    // OCI schema requires an uppercase K/M/G unit and no leading zero.
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "64kB", "limit": 1024}]}})"),
      std::runtime_error);
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "0MB", "limit": 1024}]}})"),
      std::runtime_error);
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"resources": {"hugepageLimits": [{"pageSize": "1048576B", "limit": 1024}]}})"),
      std::runtime_error);
}

TEST(ResourcesParse, HugepagePageSizeValidFormatsAccepted)
{
    const auto config = parse_config(
      R"(  "linux": {"resources": {"hugepageLimits": [
             {"pageSize": "2MB", "limit": 209715200},
             {"pageSize": "64KB", "limit": 1024},
             {"pageSize": "1GB", "limit": 1024}
           ]}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->hugepage_limits.has_value());
    const auto &limits = *config.linux_->resources_->hugepage_limits;
    ASSERT_THAT(limits, SizeIs(3));
    EXPECT_THAT(limits[0].page_size, Eq("2MB"));
    EXPECT_EQ(limits[0].limit, 209715200U);
    EXPECT_THAT(limits[1].page_size, Eq("64KB"));
    EXPECT_EQ(limits[1].limit, 1024U);
    EXPECT_THAT(limits[2].page_size, Eq("1GB"));
}

TEST(ResourcesParse, PidsLimitParsed)
{
    const auto with_limit = parse_config(R"(  "linux": {"resources": {"pids": {"limit": 42}}})");
    ASSERT_TRUE(with_limit.linux_->resources_->pids_.has_value());
    EXPECT_EQ(with_limit.linux_->resources_->pids_->limit.value_or(-1), 42);

    const auto without = parse_config(R"(  "linux": {"resources": {"pids": {}}})");
    EXPECT_FALSE(without.linux_->resources_->pids_->limit.has_value());
}

TEST(ResourcesParse, CpuMemoryBlockIoFieldsParsed)
{
    const auto config = parse_config(
      R"(  "linux": {"resources": {"cpu": {"quota": 1000, "period": 100}, "memory": {"swap": 2097152}, "blockIO": {"weight": 500, "leafWeight": 300}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    const auto &res = *config.linux_->resources_;
    ASSERT_TRUE(res.cpu_.has_value());
    EXPECT_EQ(res.cpu_->quota.value_or(0), 1000);
    EXPECT_EQ(res.cpu_->period.value_or(0), 100U);
    ASSERT_TRUE(res.memory_.has_value());
    EXPECT_EQ(res.memory_->swap.value_or(0), 2097152);
    ASSERT_TRUE(res.block_io_.has_value());
    EXPECT_EQ(res.block_io_->weight.value_or(0), 500U);
    EXPECT_EQ(res.block_io_->leaf_weight.value_or(0), 300U);
}

} // namespace
} // namespace linyaps_box
