// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/mount.h"
#include "linyaps_box/config/oci_config.h"

namespace linyaps_box {

using namespace linyaps_box::config;

namespace {

using testing::Eq;
using testing::HasSubstr;
using testing::SizeIs;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

TEST(MountParse, FixtureMounts)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.mounts.has_value());
    EXPECT_THAT(*config.mounts, SizeIs(4));
    const auto &proc_mount = (*config.mounts)[0];
    // Compare the string form: gtest 1.8.1 treats std::filesystem::path as a
    // container (path has begin()/end()) and recurses forever when printing
    // it, even for a passing matcher. Path-valued assertions elsewhere in the
    // tests follow the same .string() convention.
    EXPECT_THAT(proc_mount.destination.string(), Eq("/proc"));
    EXPECT_THAT(proc_mount.type, Eq("proc"));

    const auto &bind_mount = (*config.mounts)[1];
    EXPECT_THAT(bind_mount.source, Eq("/tmp/data"));
    EXPECT_TRUE(bind_mount.vfs_flags.contains(vfs_flag::bind));
    EXPECT_TRUE(bind_mount.vfs_flags.contains(vfs_flag::ro));

    const auto &rw_mount = (*config.mounts)[2];
    EXPECT_TRUE(rw_mount.vfs_flags.contains(vfs_flag::nosuid));
    EXPECT_FALSE(rw_mount.vfs_flags.contains(vfs_flag::ro));
    EXPECT_TRUE(rw_mount.vfs_flags.contains(vfs_flag::noatime));

    const auto &rec_mount = (*config.mounts)[3];
    EXPECT_TRUE(rec_mount.vfs_flags.contains(vfs_flag::bind));
    EXPECT_TRUE(rec_mount.vfs_flags.contains(vfs_flag::rec));
    EXPECT_TRUE(rec_mount.propagation_flags.contains(propagation_flag::private_));
    EXPECT_TRUE(rec_mount.propagation_flags.contains(propagation_flag::rec));
    ASSERT_TRUE(rec_mount.uid_mappings.has_value());
    EXPECT_EQ((*rec_mount.uid_mappings)[0].container_id, 0U);
    EXPECT_EQ((*rec_mount.uid_mappings)[0].host_id, 1000U);
}

TEST(MountParse, UnknownMountOptionFallsToData)
{
    const auto config =
      parse_config(R"(  "mounts": [{"destination": "/mnt", "source": "/tmp", "type": "none",
              "options": ["ro", "mode=0755"]}])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto &mnt = (*config.mounts)[0];
    EXPECT_TRUE(mnt.vfs_flags.contains(vfs_flag::ro));
    EXPECT_THAT(mnt.data, HasSubstr("mode=0755"));
    EXPECT_THAT(mnt.data, Not(HasSubstr("ro")));
}

TEST(MountParse, RecognizedOptionsNotInData)
{
    const auto config =
      parse_config(R"(  "mounts": [{"destination": "/mnt", "source": "/tmp", "type": "none",
              "options": ["nosuid", "strictatime", "mode=755", "size=65536k"]}])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto &mnt = (*config.mounts)[0];
    EXPECT_TRUE(mnt.vfs_flags.contains(vfs_flag::nosuid));
    EXPECT_TRUE(mnt.vfs_flags.contains(vfs_flag::strictatime));
    EXPECT_THAT(mnt.data, HasSubstr("mode=755"));
    EXPECT_THAT(mnt.data, HasSubstr("size=65536k"));
    EXPECT_THAT(mnt.data, Not(HasSubstr("nosuid")));
    EXPECT_THAT(mnt.data, Not(HasSubstr("strictatime")));
}

TEST(MountParse, MissingDestinationRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [{"source": "/tmp", "type": "none"}])"),
                 std::runtime_error);
}

TEST(MountParse, RelativeDestinationRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   R"(  "mounts": [{"destination": "mnt", "source": "/tmp", "type": "none"}])"),
                 std::runtime_error);
}

// uidMappings/gidMappings are spec v1.3.0 fields on mounts; they are parsed and
// must be given together. The inline `idmap=…` option form is not in the spec
// and is gone (such tokens now fall through as unknown options).
TEST(MountParse, UidMappingsWithoutGidMappingsRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none",
     "uidMappings": [{"containerID": 0, "hostID": 1000, "size": 1}]}
  ])"),
                 std::runtime_error);
}

TEST(MountParse, IdmapFlagsRecognized)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": ["idmap"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto &mnt = (*config.mounts)[0];
    ASSERT_TRUE(mnt.idmap.has_value());
    EXPECT_EQ(*mnt.idmap, mount::idmap_type::idmap);
    // A recognized option is not passed through to the data string.
    EXPECT_THAT(mnt.data, Not(HasSubstr("idmap")));
}

TEST(MountParse, RidmapFlagRecognized)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": ["ridmap"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto &mnt = (*config.mounts)[0];
    ASSERT_TRUE(mnt.idmap.has_value());
    EXPECT_EQ(*mnt.idmap, mount::idmap_type::ridmap);
}

TEST(MountParse, IdmapAndRidmapMutuallyExclusive)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": ["idmap", "ridmap"]}
  ])"),
                 std::runtime_error);
}

// The inline `idmap=uids=…,gids=…` form is not part of the OCI spec: it is not
// decoded, and the whole token is carried into the data string as unknown.
TEST(MountParse, InlineIdmapFormFallsToData)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none",
     "options": ["ro", "idmap=uids=0:1000:1,gids=0:1000:1"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto &mnt = (*config.mounts)[0];
    EXPECT_TRUE(mnt.vfs_flags.contains(vfs_flag::ro));
    EXPECT_FALSE(mnt.uid_mappings.has_value());
    EXPECT_FALSE(mnt.gid_mappings.has_value());
    EXPECT_FALSE(mnt.idmap.has_value());
    EXPECT_THAT(mnt.data, HasSubstr("idmap=uids=0:1000:1,gids=0:1000:1"));
}

TEST(MountParse, RecAttrOptionsParsed)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none",
     "options": ["rro", "rnosuid", "rrw"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    ASSERT_TRUE((*config.mounts)[0].rec_attr.has_value());
    const auto &attr = *(*config.mounts)[0].rec_attr;
    EXPECT_TRUE(attr.set.contains(recursive_attr_flag::rdonly));
    EXPECT_TRUE(attr.set.contains(recursive_attr_flag::nosuid));
    EXPECT_TRUE(attr.clr.contains(recursive_attr_flag::rdonly));
}

TEST(MountParse, RecAttrClrOnlyParsed)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none",
     "options": ["rrw"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    ASSERT_TRUE((*config.mounts)[0].rec_attr.has_value());
    EXPECT_TRUE((*config.mounts)[0].rec_attr->set.empty());
    EXPECT_TRUE((*config.mounts)[0].rec_attr->clr.contains(recursive_attr_flag::rdonly));
}

TEST(MountParse, ExtensionOptionsParsed)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none",
     "options": ["copy-symlink", "tmpcopyup"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    const auto flags = (*config.mounts)[0].extension_flags;
    EXPECT_NE(flags & mount::extension::copy_symlink, mount::extension::none);
    EXPECT_NE(flags & mount::extension::tmpcopyup, mount::extension::none);
}

TEST(MountParse, OptionsMustBeAnArray)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": "ro"}
  ])"),
                 std::runtime_error);
}

TEST(MountParse, OptionsElementMustBeAString)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": [{"ro": true}]}
  ])"),
                 std::runtime_error);

    EXPECT_THROW(std::ignore = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": [["ro"]]}
  ])"),
                 std::runtime_error);
}

TEST(MountParse, NullOptionsAccepted)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": null}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    EXPECT_EQ((*config.mounts)[0].destination.string(), "/mnt");
}

TEST(MountParse, UnknownOptionBeforeKnownEntriesFallsToData)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": ["aaa"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    EXPECT_THAT((*config.mounts)[0].data, HasSubstr("aaa"));
}

TEST(MountParse, UnknownOptionAfterKnownEntriesFallsToData)
{
    const auto config = parse_config(R"(  "mounts": [
    {"destination": "/mnt", "source": "/tmp", "type": "none", "options": ["zzz"]}
  ])");
    ASSERT_TRUE(config.mounts.has_value());
    EXPECT_THAT((*config.mounts)[0].data, HasSubstr("zzz"));
}

} // namespace
} // namespace linyaps_box
