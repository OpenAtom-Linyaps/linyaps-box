// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

#include <filesystem>
#include <string>

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::Eq;
using testing::SizeIs;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

using namespace std::string_view_literals;

TEST(OciConfigParse, FixtureTopLevelFields)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    EXPECT_THAT(config.version, Eq("1.3.0"));
    EXPECT_THAT(config.hostname, Eq("test-host"));
    EXPECT_THAT(config.domainname, Eq("test-domain"));

    ASSERT_TRUE(config.annotations.has_value());
    EXPECT_THAT(config.annotations->at("org.example.key"), Eq("value"));
}

TEST(OciConfigParse, FixtureHooks)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.hooks_.has_value());
    const auto &hooks = *config.hooks_;
    ASSERT_TRUE(hooks.prestart.has_value());
    EXPECT_THAT(*hooks.prestart, SizeIs(1));
    EXPECT_THAT((*hooks.prestart)[0].path.string(), Eq("/bin/prestart"));
}

TEST(OciConfigParse, MissingOciVersionRejected)
{
    const auto content = R"({
  "process": {"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}}
})"sv;
    EXPECT_THROW(oci_config::parse(content), std::runtime_error);
}

TEST(OciConfigParse, UnsupportedOciVersionRejected)
{
    const auto content = R"({
  "ociVersion": "2.0.0",
  "process": {"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}}
})"sv;
    EXPECT_THROW(oci_config::parse(std::string_view{ content }), std::runtime_error);
}

TEST(OciConfigParse, AcceptsConfigWithoutProcess)
{
    const auto config = parse_config(R"("root": {"path": "rootfs"},
  "hostname": "no-process")",
                                     "");
    EXPECT_FALSE(config.process_.has_value());
}

TEST(OciConfigParse, EmptyAnnotationKeyRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "annotations": {"": "empty-key"})"),
                 std::runtime_error);
}

TEST(OciConfigParse, NullProcessAccepted)
{
    const auto config = parse_config(R"("root": {"path": "rootfs"})", "null");
    EXPECT_FALSE(config.process_.has_value());
}

TEST(OciConfigParse, NullMountsAccepted)
{
    const auto config = parse_config(R"(  "mounts": null)");
    EXPECT_TRUE(config.mounts.empty());
}

TEST(OciConfigParse, UnknownTopLevelKeyIgnored)
{
    const auto config = parse_config(R"(  "x-extension": {"anything": true})");
    EXPECT_THAT(config.version, Eq("1.3.0"));
}

TEST(OciConfigParse, MissingConfigFileThrows)
{
    EXPECT_THROW(oci_config::parse(std::filesystem::path{ "/nonexistent/config.json" }),
                 std::runtime_error);
}

TEST(OciConfigParse, EmptyRootPathRejected)
{
    EXPECT_THROW(std::ignore = parse_config(R"(  "root": {"path": ""})"), std::runtime_error);
}

TEST(OciConfigParse, OfficialSpecExampleParses)
{
    const auto content = load_fixture("data/spec-example.json");
    const auto config = oci_config::parse(std::string_view{ content });

    EXPECT_THAT(config.version, Eq("1.3.0"));
    ASSERT_TRUE(config.process_.has_value());
    EXPECT_TRUE(config.process_->terminal.value_or(false));
    EXPECT_EQ(config.process_->user_.uid, 1U);
    EXPECT_THAT(config.process_->cwd.string(), Eq("/"));
    ASSERT_TRUE(config.linux_.has_value());
    ASSERT_TRUE(config.linux_->namespaces.has_value());
    EXPECT_FALSE(config.mounts.empty());
}

TEST(OciConfigParse, ParseFromFileSucceeds)
{
    const auto config = oci_config::parse(std::filesystem::path{ "data/oci.json" });
    EXPECT_THAT(config.hostname, Eq("test-host"));
    ASSERT_TRUE(config.process_.has_value());
}

TEST(OciConfigParse, ParseDirectoryThrows)
{
    EXPECT_THROW(oci_config::parse(std::filesystem::path{ "data" }), std::runtime_error);
}

} // namespace
} // namespace linyaps_box
