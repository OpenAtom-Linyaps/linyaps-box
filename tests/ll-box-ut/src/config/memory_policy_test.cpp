// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

#include <fmt/format.h>

#include <tuple>

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using linyaps_box::test::parse_config;

[[nodiscard]] auto policy_json(const char *mode, const char *nodes, const char *flags)
  -> std::string
{
    auto policy = fmt::format(R"({{"mode": "{}")", mode);
    if (nodes != nullptr) {
        policy += fmt::format(R"(, "nodes": "{}")", nodes);
    }

    if (flags != nullptr) {
        policy += fmt::format(R"(, "flags": {})", flags);
    }

    policy += "}";
    return fmt::format(R"(  "linux": {{"memoryPolicy": {}}})", policy);
}

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
    EXPECT_EQ(*config.linux_->memory_policy_->nodes, "0,2");
    EXPECT_TRUE(config.linux_->memory_policy_->flags.contains(memory_policy_flag::static_nodes));
}

TEST(MemoryPolicyParse, NullPolicyAccepted)
{
    const auto config = parse_config(R"(  "linux": {"memoryPolicy": null})");
    ASSERT_TRUE(config.linux_.has_value());
    EXPECT_FALSE(config.linux_->memory_policy_.has_value());
}

TEST(MemoryPolicyParse, NodesKeptVerbatim)
{
    for (const auto *const nodes : { "", "   ", ", ,", "0-3,7", "all", "1-123456789123456789" }) {
        const auto config = parse_config(policy_json("MPOL_PREFERRED", nodes, nullptr));
        ASSERT_TRUE(config.linux_.has_value()) << nodes;
        ASSERT_TRUE(config.linux_->memory_policy_.has_value()) << nodes;
        ASSERT_TRUE(config.linux_->memory_policy_->nodes.has_value()) << nodes;
        EXPECT_EQ(*config.linux_->memory_policy_->nodes, nodes) << nodes;
    }
}

class MemoryPolicyModeNodesTest
    : public testing::TestWithParam<std::tuple<const char *, const char *, bool>>
{
};

TEST_P(MemoryPolicyModeNodesTest, ModeNodesConsistency)
{
    const auto &[mode, nodes, rejected] = GetParam();
    const auto content = policy_json(mode, nodes, nullptr);

    if (rejected) {
        EXPECT_THROW(std::ignore = parse_config(content), std::runtime_error) << content;
    } else {
        EXPECT_NO_THROW(std::ignore = parse_config(content)) << content;
    }
}

INSTANTIATE_TEST_SUITE_P(MemoryPolicyParse,
                         MemoryPolicyModeNodesTest,
                         testing::Values(std::tuple{ "MPOL_DEFAULT", nullptr, false },
                                         std::tuple{ "MPOL_DEFAULT", "", false },
                                         std::tuple{ "MPOL_DEFAULT", "   ", false },
                                         std::tuple{ "MPOL_DEFAULT", ", ,", false },
                                         std::tuple{ "MPOL_DEFAULT", "0,2", true },
                                         std::tuple{ "MPOL_LOCAL", "0", true },
                                         std::tuple{ "MPOL_LOCAL", "   ", false },
                                         std::tuple{ "MPOL_BIND", nullptr, true },
                                         std::tuple{ "MPOL_BIND", "", true },
                                         std::tuple{ "MPOL_BIND", "   ", true },
                                         std::tuple{ "MPOL_BIND", ", ,", true },
                                         std::tuple{ "MPOL_BIND", "0,2", false },
                                         std::tuple{ "MPOL_BIND", "all", false },
                                         std::tuple{ "MPOL_INTERLEAVE", nullptr, true },
                                         std::tuple{ "MPOL_PREFERRED_MANY", nullptr, true },
                                         std::tuple{ "MPOL_WEIGHTED_INTERLEAVE", nullptr, true },
                                         std::tuple{ "MPOL_PREFERRED", nullptr, false },
                                         std::tuple{ "MPOL_PREFERRED", "", false },
                                         std::tuple{ "MPOL_PREFERRED", "0,2", false }));

class MemoryPolicyFlagsTest
    : public testing::TestWithParam<std::tuple<const char *, const char *, const char *, bool>>
{
};

TEST_P(MemoryPolicyFlagsTest, FlagConsistency)
{
    const auto &[mode, nodes, flags, rejected] = GetParam();
    const auto content = policy_json(mode, nodes, flags);

    if (rejected) {
        EXPECT_THROW(std::ignore = parse_config(content), std::runtime_error) << content;
    } else {
        EXPECT_NO_THROW(std::ignore = parse_config(content)) << content;
    }
}

INSTANTIATE_TEST_SUITE_P(
  MemoryPolicyParse,
  MemoryPolicyFlagsTest,
  testing::Values(
    // MPOL_F_STATIC_NODES and MPOL_F_RELATIVE_NODES are mutually exclusive.
    std::tuple{ "MPOL_BIND", "0", R"(["MPOL_F_STATIC_NODES", "MPOL_F_RELATIVE_NODES"])", true },
    std::tuple{ "MPOL_BIND", "0", R"(["MPOL_F_STATIC_NODES"])", false },
    // MPOL_F_NUMA_BALANCING is valid with MPOL_BIND and MPOL_PREFERRED_MANY only.
    std::tuple{ "MPOL_BIND", "0", R"(["MPOL_F_NUMA_BALANCING"])", false },
    std::tuple{ "MPOL_PREFERRED_MANY", "0", R"(["MPOL_F_NUMA_BALANCING"])", false },
    std::tuple{ "MPOL_INTERLEAVE", "0", R"(["MPOL_F_NUMA_BALANCING"])", true },
    std::tuple{ "MPOL_PREFERRED", "0", R"(["MPOL_F_NUMA_BALANCING"])", true },
    // The kernel ignores flags for MPOL_DEFAULT, but rejects them for MPOL_LOCAL.
    std::tuple{ "MPOL_DEFAULT", nullptr, R"(["MPOL_F_STATIC_NODES"])", false },
    std::tuple{ "MPOL_LOCAL", nullptr, R"(["MPOL_F_STATIC_NODES"])", true },
    // MPOL_PREFERRED with empty nodes rejects static/relative flags.
    std::tuple{ "MPOL_PREFERRED", nullptr, R"(["MPOL_F_STATIC_NODES"])", true },
    std::tuple{ "MPOL_PREFERRED", "0", R"(["MPOL_F_STATIC_NODES"])", false }));

} // namespace
} // namespace linyaps_box
