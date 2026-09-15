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

TEST(CpuParse, IdleValuesParsed)
{
    const auto idle = parse_config(R"(  "linux": {"resources": {"cpu": {"idle": 1}}})");
    ASSERT_TRUE(idle.linux_->resources_->cpu_->idle_.has_value());
    EXPECT_EQ(*idle.linux_->resources_->cpu_->idle_, cpu::idle::idle);

    const auto none = parse_config(R"(  "linux": {"resources": {"cpu": {"idle": 0}}})");
    ASSERT_TRUE(none.linux_->resources_->cpu_->idle_.has_value());
    EXPECT_EQ(*none.linux_->resources_->cpu_->idle_, cpu::idle::none);
}

TEST(CpuParse, CpusWrongTypeRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"resources": {"cpu": {"cpus": 42}}})"),
                 std::exception);
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

class CpuEmptyCpusTest : public testing::TestWithParam<const char *>
{
};

TEST_P(CpuEmptyCpusTest, YieldsEmpty)
{
    const auto content = fmt::format(
      R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"resources": {{"cpu": {{"cpus": "{}"}}}}}}
}})",
      GetParam());

    const auto config = oci_config::parse(std::string_view{ content });
    ASSERT_TRUE(config.linux_->resources_->cpu_->cpus.has_value());
    EXPECT_TRUE(config.linux_->resources_->cpu_->cpus->empty());
}

INSTANTIATE_TEST_SUITE_P(CpuParse, CpuEmptyCpusTest, testing::Values("", "   "));

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
