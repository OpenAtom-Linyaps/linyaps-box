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

using linyaps_box::test::parse_config;

TEST(MemoryPolicyParse, UnknownModeRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_NOPE"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, UnknownFlagRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"memoryPolicy": {"mode": "MPOL_BIND", "flags": ["MPOL_F_NOPE"]}})"),
      std::runtime_error);
}

TEST(MemoryPolicyParse, HappyPath)
{
    const auto config = parse_config(
      R"(  "linux": {"memoryPolicy": {"mode": "MPOL_BIND", "nodes": "0,2", "flags": ["MPOL_F_STATIC_NODES"]}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->memory_policy_.has_value());
    EXPECT_EQ(config.linux_->memory_policy_->mode_, memory_policy::mode::bind);
    ASSERT_TRUE(config.linux_->memory_policy_->nodes.has_value());
    EXPECT_THAT(*config.linux_->memory_policy_->nodes, ElementsAre(0, 2));
    EXPECT_TRUE(config.linux_->memory_policy_->flags.contains(memory_policy_flag::static_nodes));
}

TEST(MemoryPolicyParse, NullPolicyAccepted)
{
    const auto config = parse_config(R"(  "linux": {"memoryPolicy": null})");
    ASSERT_TRUE(config.linux_.has_value());
    EXPECT_FALSE(config.linux_->memory_policy_.has_value());
}

TEST(MemoryPolicyParse, DefaultModeWithNodesRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"memoryPolicy": {"mode": "MPOL_DEFAULT", "nodes": "0,2"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, LocalModeWithNodesRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"memoryPolicy": {"mode": "MPOL_LOCAL", "nodes": "0,2"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, BindModeRequiresNodes)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_BIND"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, InterleaveModeRequiresNodes)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_INTERLEAVE"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, PreferredManyModeRequiresNodes)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_PREFERRED_MANY"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, WeightedInterleaveModeRequiresNodes)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"memoryPolicy": {"mode": "MPOL_WEIGHTED_INTERLEAVE"}})"),
                 std::runtime_error);
}

TEST(MemoryPolicyParse, PreferredModeAllowsAnyNodes)
{
    const auto no_nodes =
      parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_PREFERRED"}})");
    ASSERT_TRUE(no_nodes.linux_.has_value());
    ASSERT_TRUE(no_nodes.linux_->memory_policy_.has_value());
    EXPECT_FALSE(no_nodes.linux_->memory_policy_->nodes.has_value());

    const auto with_nodes =
      parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_PREFERRED", "nodes": "2"}})");
    ASSERT_TRUE(with_nodes.linux_.has_value());
    ASSERT_TRUE(with_nodes.linux_->memory_policy_.has_value());
    ASSERT_TRUE(with_nodes.linux_->memory_policy_->nodes.has_value());
}

TEST(MemoryPolicyParse, DefaultModeWithoutNodesAccepted)
{
    const auto config = parse_config(R"(  "linux": {"memoryPolicy": {"mode": "MPOL_DEFAULT"}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->memory_policy_.has_value());
    EXPECT_FALSE(config.linux_->memory_policy_->nodes.has_value());
}

} // namespace
} // namespace linyaps_box
