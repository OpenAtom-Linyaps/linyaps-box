// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

#include <fmt/format.h>

#include <array>
#include <utility>

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::ElementsAre;
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
    EXPECT_EQ(linux.network_devices->count("eth0"), 1U);
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
    const std::array cases = {
        std::pair{ "private", rootfs_propagation::private_ },
        std::pair{ "shared", rootfs_propagation::shared },
        std::pair{ "slave", rootfs_propagation::slave },
        std::pair{ "unbindable", rootfs_propagation::unbindable },
    };

    for (const auto &[value, expected] : cases) {
        const auto content = fmt::format(
          R"({{
  "ociVersion": "1.3.0",
  "process": {{"cwd": "/", "args": ["/bin/true"], "user": {{"uid": 0, "gid": 0}}}},
  "linux": {{"rootfsPropagation": "{}"}}
}})",
          value);

        const auto config = oci_config::parse(std::string_view{ content });
        ASSERT_TRUE(config.linux_.has_value());
        ASSERT_TRUE(config.linux_->rootfs_propagation_.has_value());
        EXPECT_EQ(*config.linux_->rootfs_propagation_, expected);
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

TEST(LinuxParse, IntelRdtMemBwSchemaInvalidRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"intelRdt": {"memBwSchema": "MBX:0=1"}})"),
                 std::runtime_error);
}

TEST(LinuxParse, IntelRdtSchemataNewlineRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config(R"(  "linux": {"intelRdt": {"schemata": ["L3:0=7f0\nL2:0=f"]}})"),
                 std::runtime_error);
}

TEST(LinuxParse, IntelRdtValidAccepted)
{
    const auto config = parse_config(
      R"(  "linux": {"intelRdt": {"closID": "group1", "l3CacheSchema": "L3:0=7f0", "memBwSchema": "MB:0=20", "schemata": ["L3:0=7f0"], "enableMonitoring": true}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->intel_rdt_.has_value());
    EXPECT_THAT(config.linux_->intel_rdt_->clos_id.value_or(""), Eq("group1"));
    EXPECT_EQ(config.linux_->intel_rdt_->enable_monitoring.value_or(false), true);
}

// The fields below are OPTIONAL per the spec; assert the parsed values as well
// as the omitted (absent) form.
TEST(LinuxParse, NetDeviceNamesParsed)
{
    const auto config =
      parse_config(R"(  "linux": {"netDevices": {"eth0": {"name": "veth0"}, "ens4": {}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->network_devices.has_value());
    EXPECT_THAT(config.linux_->network_devices->at("eth0").name.value_or(""), Eq("veth0"));
    EXPECT_FALSE(config.linux_->network_devices->at("ens4").name.has_value());
}

TEST(LinuxParse, IntelRdtValuesParsed)
{
    const auto config = parse_config(
      R"(  "linux": {"intelRdt": {"closID": "group1", "l3CacheSchema": "L3:0=7f0", "enableMonitoring": true}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->intel_rdt_.has_value());
    const auto &rdt = *config.linux_->intel_rdt_;
    EXPECT_THAT(rdt.clos_id.value_or(""), Eq("group1"));
    EXPECT_THAT(rdt.l3_cache_schema.value_or(""), Eq("L3:0=7f0"));
    EXPECT_EQ(rdt.enable_monitoring.value_or(false), true);
    EXPECT_FALSE(rdt.memory_bandwidth_schema.has_value());
}

TEST(LinuxParse, PidsLimitParsed)
{
    const auto with_limit = parse_config(R"(  "linux": {"resources": {"pids": {"limit": 42}}})");
    ASSERT_TRUE(with_limit.linux_->resources_->pids_.has_value());
    EXPECT_EQ(with_limit.linux_->resources_->pids_->limit.value_or(-1), 42);

    const auto without = parse_config(R"(  "linux": {"resources": {"pids": {}}})");
    EXPECT_FALSE(without.linux_->resources_->pids_->limit.has_value());
}

TEST(LinuxParse, PersonalityDomainAndFlagsParsed)
{
    const auto config = parse_config(
      R"(  "linux": {"personality": {"domain": "LINUX32", "flags": ["ADDR_NO_RANDOMIZE"]}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->personality_.has_value());
    EXPECT_EQ(config.linux_->personality_->domain_, personality::domain::linux32);
    ASSERT_TRUE(config.linux_->personality_->flags.has_value());
    EXPECT_THAT(*config.linux_->personality_->flags, ElementsAre("ADDR_NO_RANDOMIZE"));
}

TEST(LinuxParse, CpuMemoryBlockIoValuesParsed)
{
    const auto config = parse_config(
      R"(  "linux": {"resources": {"cpu": {"shares": 512, "quota": 1000, "period": 100}, "memory": {"limit": 1048576, "swap": 2097152}, "blockIO": {"weight": 500, "leafWeight": 300}}})");
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->resources_.has_value());
    const auto &res = *config.linux_->resources_;
    ASSERT_TRUE(res.cpu_.has_value());
    EXPECT_EQ(res.cpu_->shares.value_or(0), 512U);
    EXPECT_EQ(res.cpu_->quota.value_or(0), 1000);
    EXPECT_EQ(res.cpu_->period.value_or(0), 100U);
    ASSERT_TRUE(res.memory_.has_value());
    EXPECT_EQ(res.memory_->limit.value_or(0), 1048576);
    EXPECT_EQ(res.memory_->swap.value_or(0), 2097152);
    ASSERT_TRUE(res.block_io_.has_value());
    EXPECT_EQ(res.block_io_->weight.value_or(0), 500U);
    EXPECT_EQ(res.block_io_->leaf_weight.value_or(0), 300U);
}

} // namespace
} // namespace linyaps_box
