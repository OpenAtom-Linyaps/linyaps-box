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

using testing::Eq;
using testing::SizeIs;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

TEST(LinuxParse, FixtureLinux)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.linux_.has_value());
    const auto &linux = *config.linux_;
    ASSERT_TRUE(linux.namespaces.has_value());
    EXPECT_THAT(*linux.namespaces, SizeIs(4));
    EXPECT_EQ((*linux.namespaces)[0].type_, ns::type::pid);
    EXPECT_EQ((*linux.namespaces)[1].type_, ns::type::mount);

    ASSERT_TRUE(linux.rootfs_propagation_.has_value());
    EXPECT_EQ(*linux.rootfs_propagation_, rootfs_propagation::slave);

    ASSERT_TRUE(linux.network_devices.has_value());
    EXPECT_TRUE(linux.network_devices->count("eth0") == 1);
    EXPECT_THAT(linux.network_devices->at("eth0").name, Eq("veth0"));

    ASSERT_TRUE(linux.devices.has_value());
    EXPECT_THAT(*linux.devices, SizeIs(1));
    EXPECT_EQ((*linux.devices)[0].mode.value_or(0), 438U); // spec "fileMode"
}

TEST(LinuxParse, InvalidDeviceTypeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"devices": [{"type": "x", "path": "/dev/foo", "major": 1, "minor": 3}]})"),
      std::runtime_error);
}

TEST(LinuxParse, DeviceMissingMajorMinorRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"devices": [{"type": "c", "path": "/dev/foo"}]})"),
                 std::runtime_error);
}

TEST(LinuxParse, RootfsPropagationSpecValues)
{
    for (const char *value : { "private", "shared", "slave", "unbindable" }) {
        const auto content = fmt::format(
          R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"rootfsPropagation": "{}"}}
}})",
          value);

        const auto config = oci_config::parse(std::string_view{ content });
        ASSERT_TRUE(config.linux_.has_value());
        EXPECT_TRUE(config.linux_->rootfs_propagation_.has_value());
    }
}

TEST(LinuxParse, RootfsPropagationRejectsRuncExtension)
{
    for (const char *value : { "rprivate", "rshared", "rslave", "runbindable" }) {
        const auto content = fmt::format(
          R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"rootfsPropagation": "{}"}}
}})",
          value);

        EXPECT_THROW(oci_config::parse(std::string_view{ content }), std::runtime_error)
          << "rootfsPropagation value not rejected: " << value;
    }
}

TEST(LinuxParse, RelativeMaskedPathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"maskedPaths": ["relative/path"]})"),
                 std::runtime_error);
}

TEST(LinuxParse, RelativeReadonlyPathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"readonlyPaths": ["relative/path"]})"),
                 std::runtime_error);
}

TEST(LinuxParse, UnknownPersonalityDomainRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "linux": {"personality": {"domain": "NOPE"}})"),
                 std::runtime_error);
}

TEST(LinuxParse, DeviceMissingTypeRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"devices": [{"path": "/dev/foo", "major": 1, "minor": 3}]})"),
                 std::runtime_error);
}

TEST(LinuxParse, DeviceMissingPathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"devices": [{"type": "c", "major": 1, "minor": 3}]})"),
                 std::runtime_error);
}

TEST(LinuxParse, DeviceRelativePathRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        R"(  "linux": {"devices": [{"type": "c", "path": "relative", "major": 1, "minor": 3}]})"),
      std::runtime_error);
}

TEST(LinuxParse, DuplicateDeviceRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "linux": {"devices": [
                         {"type": "c", "path": "/dev/foo", "major": 1, "minor": 3},
                         {"type": "c", "path": "/dev/foo2", "major": 1, "minor": 3}
                       ]})"),
                 std::runtime_error);
}

TEST(LinuxParse, TimeOffsetSecsOnlyAccepted)
{
    const auto config =
      parse_config(R"(  "linux": {"timeOffsets": {"monotonic": {"secs": 172800}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->time_offsets.has_value());
    const auto &offset = config.linux_->time_offsets->at("monotonic");
    EXPECT_EQ(offset.secs, 172800);
    EXPECT_FALSE(offset.nanosecs.has_value());
}

TEST(LinuxParse, TimeOffsetEmptyAccepted)
{
    const auto config = parse_config(R"(  "linux": {"timeOffsets": {"monotonic": {}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->time_offsets.has_value());
    const auto &offset = config.linux_->time_offsets->at("monotonic");
    EXPECT_FALSE(offset.secs.has_value());
    EXPECT_FALSE(offset.nanosecs.has_value());
}

} // namespace
} // namespace linyaps_box
