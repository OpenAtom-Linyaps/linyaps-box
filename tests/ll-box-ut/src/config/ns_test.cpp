// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/ns.h"
#include "linyaps_box/config/oci_config.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/setns.h"

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::HasSubstr;

using linyaps_box::test::parse_config;

TEST(NsParse, UnknownNamespaceTypeThrows)
{
    try {
        std::ignore = parse_config(R"(  "linux": {"namespaces": [{"type": "nope"}]})");
        FAIL() << "expected parse to throw";
    } catch (const std::runtime_error &e) {
        EXPECT_THAT(e.what(), HasSubstr("nope"));
    }
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

TEST(NsParse, NsWithPathParses)
{
    const auto config =
      parse_config(R"(  "linux": {"namespaces": [{"type": "pid", "path": "/proc/self/ns/pid"}]})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->namespaces.has_value());
    ASSERT_TRUE((*config.linux_->namespaces)[0].path.has_value());
}

TEST(NsParse, UserNsPathWithUidMappingsRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"namespaces": [{"type": "user", "path": "/proc/self/ns/user"}],
            "uidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}]})"),
                 std::runtime_error);
}

TEST(NsParse, UserNsWithoutPathWithUidMappingsParses)
{
    EXPECT_NO_THROW(std::ignore = parse_config(R"(  "linux": {"namespaces": [{"type": "user"}],
            "uidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}],
            "gidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}]})"));
}

TEST(NsParse, VerifyNamespaceFdMatches)
{
    auto fd = os::throw_if_error(
      os::open("/proc/self/ns/pid",
               { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));
    EXPECT_NO_THROW(utils::verify_namespace_fd(fd, ns::type::pid));
}

TEST(NsParse, VerifyNamespaceFdTypeMismatchThrows)
{
    auto fd = os::throw_if_error(
      os::open("/proc/self/ns/pid",
               { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));
    EXPECT_THROW(utils::verify_namespace_fd(fd, ns::type::net), std::runtime_error);
}

TEST(NsParse, VerifyNamespaceFdNotNamespaceThrows)
{
    auto fd = os::throw_if_error(
      os::open("/etc/hosts", { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));
    EXPECT_THROW(utils::verify_namespace_fd(fd, ns::type::pid), std::runtime_error);
}

} // namespace
} // namespace linyaps_box
