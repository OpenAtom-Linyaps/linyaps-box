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

TEST(CpuParse, CpusKeptVerbatim)
{
    for (const auto *const cpus : { "", "   ", "0-3,7", "1,,2", "1-4294967294" }) {
        const auto content = fmt::format(
          R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"resources": {{"cpu": {{"cpus": "{}"}}}}}}
}})",
          cpus);

        const auto config = oci_config::parse(std::string_view{ content });
        ASSERT_TRUE(config.linux_->resources_->cpu_->cpus.has_value());
        EXPECT_EQ(*config.linux_->resources_->cpu_->cpus, cpus) << cpus;
    }
}

TEST(CpuParse, QuotaSmallerThanBurstRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"resources": {"cpu": {"quota": 100, "burst": 200}}})"),
                 std::runtime_error);
}

} // namespace
} // namespace linyaps_box
