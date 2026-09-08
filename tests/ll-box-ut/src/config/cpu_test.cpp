// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

#include <fmt/format.h>

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::ElementsAre;

using linyaps_box::test::parse_config;

TEST(CpuParse, IdleOutOfRangeRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"resources": {"cpu": {"idle": 2}}})"),
                 std::runtime_error);
}

TEST(CpuParse, IdleEnumTableRegistered)
{
    EXPECT_TRUE(get_enum_table_from<cpu::idle>().from_name("idle").has_value());
    EXPECT_TRUE(get_enum_table_from<cpu::idle>().from_name("none").has_value());
    EXPECT_FALSE(get_enum_table_from<cpu::idle>().from_name("nope").has_value());
}

TEST(CpuParse, CpusWrongTypeRejected)
{
    EXPECT_ANY_THROW(std::ignore =
                       parse_config(R"(  "linux": {"resources": {"cpu": {"cpus": 42}}})"));
}

class CpuParseRangeListTest : public testing::TestWithParam<const char *>
{
};

TEST_P(CpuParseRangeListTest, InvalidCpusRejected)
{
    const auto *const cpus = GetParam();
    const auto content = fmt::format(
      R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"resources": {{"cpu": {{"cpus": "{}"}}}}}}
}})",
      cpus);
    EXPECT_THROW(oci_config::parse(std::string_view{ content }), std::runtime_error)
      << "cpus not rejected: " << cpus;
}

INSTANTIATE_TEST_SUITE_P(CpuParse,
                         CpuParseRangeListTest,
                         testing::Values("x",
                                         "1x",
                                         "1,x",
                                         "1-",
                                         "-3",
                                         "3-1",
                                         "0-4294967295",
                                         "99999999999999999999",
                                         "0-99999999999999999999",
                                         "1 2",
                                         "1-3 5"));

TEST(CpuParse, EmptyCpusYieldsEmpty)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"cpu": {"cpus": ""}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_->cpus.has_value());
    EXPECT_TRUE(config.linux_->resources_->cpu_->cpus->empty());
}

TEST(CpuParse, WhitespaceCpusYieldsEmpty)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"cpu": {"cpus": "   "}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_->cpus.has_value());
    EXPECT_TRUE(config.linux_->resources_->cpu_->cpus->empty());
}

TEST(CpuParse, QuotaSmallerThanBurstRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"resources": {"cpu": {"quota": 100, "burst": 200}}})"),
                 std::runtime_error);
}

TEST(CpuParse, CpusHappyPath)
{
    const auto config = parse_config(R"(  "linux": {"resources": {"cpu": {"cpus": "0-3,7"}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_.has_value());
    ASSERT_TRUE(config.linux_->resources_->cpu_->cpus.has_value());
    EXPECT_THAT(*config.linux_->resources_->cpu_->cpus, ElementsAre(0, 1, 2, 3, 7));
}

} // namespace
} // namespace linyaps_box
