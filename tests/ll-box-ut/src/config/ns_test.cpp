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

TEST(NsParse, UnknownNamespaceTypeRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"namespaces": [{"type": "nope"}]})"),
                 std::runtime_error);
}

TEST(NsParse, DuplicateNamespaceTypeRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"namespaces": [{"type": "pid"}, {"type": "pid"}]})"),
                 std::runtime_error);
}

TEST(NsParse, EmptyPathRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"namespaces": [{"type": "pid", "path": ""}]})"),
                 std::runtime_error);
}

TEST(NsParse, RelativePathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"namespaces": [{"type": "pid", "path": "relative"}]})"),
                 std::runtime_error);
}

TEST(NsParse, NamespacePathParsed)
{
    const auto config =
      parse_config(R"(  "linux": {"namespaces": [{"type": "pid", "path": "/proc/self/ns/pid"}]})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->namespaces.has_value());
    ASSERT_TRUE((*config.linux_->namespaces)[0].path.has_value());
    EXPECT_THAT((*config.linux_->namespaces)[0].path->string(), Eq("/proc/self/ns/pid"));
}

TEST(NsParse, UserNsPathWithUidMappingsRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"namespaces": [{"type": "user", "path": "/proc/self/ns/user"}],
            "uidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}]})"),
                 std::runtime_error);
}

TEST(NsParse, UserNsWithoutPathWithMappingsParsed)
{
    const auto config = parse_config(R"(  "linux": {"namespaces": [{"type": "user"}],
            "uidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}],
            "gidMappings": [{"containerID": 0, "hostID": 2000, "size": 2}]})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->uid_mappings.has_value());
    EXPECT_EQ((*config.linux_->uid_mappings)[0].host_id, 1000U);
    ASSERT_TRUE(config.linux_->gid_mappings.has_value());
    EXPECT_EQ((*config.linux_->gid_mappings)[0].host_id, 2000U);
    EXPECT_EQ((*config.linux_->gid_mappings)[0].size, 2U);
}

} // namespace
} // namespace linyaps_box
