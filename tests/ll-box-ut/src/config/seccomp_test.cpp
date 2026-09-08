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

using testing::Contains;
using testing::SizeIs;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

TEST(SeccompParse, FixtureSeccomp)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.linux_.has_value());
    const auto &linux = *config.linux_;
    ASSERT_TRUE(linux.seccomp_.has_value());
    EXPECT_EQ(linux.seccomp_->default_action, seccomp::action::errno_);
    ASSERT_TRUE(linux.seccomp_->syscalls.has_value());
    EXPECT_THAT(*linux.seccomp_->syscalls, SizeIs(1));
    EXPECT_THAT((*linux.seccomp_->syscalls)[0].names, Contains("chmod"));
    EXPECT_EQ((*linux.seccomp_->syscalls)[0].action_, seccomp::action::allow);
}

TEST(SeccompParse, UnknownDefaultActionRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_NOPE"}})"),
                 std::runtime_error);
}

TEST(SeccompParse, UnknownArchRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "architectures": ["SCMP_ARCH_NOPE"]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, UnknownFlagRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "flags": ["SECCOMP_FILTER_FLAG_NOPE"]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, UnknownSyscallActionRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "syscalls": [{"names": ["chmod"], "action": "SCMP_ACT_NOPE"}]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, UnknownArgOpRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "syscalls": [{"names": ["chmod"], "action": "SCMP_ACT_ALLOW",
                                      "args": [{"index": 0, "value": 3, "op": "SCMP_CMP_NOPE"}]}]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, EmptyNamesRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "syscalls": [{"names": [], "action": "SCMP_ACT_ALLOW"}]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, DefaultErrnoRetWrongActionRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW", "defaultErrnoRet": 1}})"),
      std::runtime_error);
}

TEST(SeccompParse, SyscallErrnoRetWrongActionRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "syscalls": [{"names": ["chmod"], "action": "SCMP_ACT_KILL",
                                      "errnoRet": 1}]}})"),
                 std::runtime_error);
}

TEST(SeccompParse, NotifyRequiresListenerPath)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_NOTIFY"}})"),
                 std::runtime_error);
}

TEST(SeccompParse, ListenerMetadataRequiresListenerPath)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_ALLOW",
                        "listenerMetadata": "meta"}})"),
                 std::runtime_error);
}

TEST(SeccompParse, NotifyWithListenerPathParses)
{
    EXPECT_NO_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"seccomp": {"defaultAction": "SCMP_ACT_NOTIFY", "listenerPath": "/run/sock"}})"));
}

TEST(SeccompParse, DefaultActionRequiredRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"seccomp": {"architectures": ["SCMP_ARCH_X86_64"]}})"),
                 std::runtime_error);
}

} // namespace
} // namespace linyaps_box
