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
using testing::Eq;

TEST(HookParse, RelativePathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "hooks": {"prestart": [{"path": "relative"}]})"),
                 std::runtime_error);
}

TEST(HookParse, NonPositiveTimeoutRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "hooks": {"prestart": [{"path": "/bin/prestart", "timeout": 0}]})"),
                 std::runtime_error);
}

TEST(HookParse, InvalidEnvRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "hooks": {"prestart": [{"path": "/bin/prestart", "env": ["NOEQUALS"]}]})"),
                 std::runtime_error);
}

TEST(HookParse, HappyPath)
{
    const auto config = parse_config(R"(  "hooks": {"prestart": [{"path": "/bin/prestart",
                          "args": ["/bin/prestart", "arg"],
                          "env": ["FOO=bar"],
                          "timeout": 10}]})");
    ASSERT_TRUE(config.hooks_.has_value());
    ASSERT_TRUE(config.hooks_->prestart.has_value());
    EXPECT_EQ(config.hooks_->prestart->at(0).path.string(), "/bin/prestart");
    ASSERT_TRUE(config.hooks_->prestart->at(0).args.has_value());
    EXPECT_THAT(*config.hooks_->prestart->at(0).args, ElementsAre("/bin/prestart", "arg"));
    ASSERT_TRUE(config.hooks_->prestart->at(0).timeout.has_value());
    EXPECT_EQ(*config.hooks_->prestart->at(0).timeout, 10);
}

} // namespace
} // namespace linyaps_box
