// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fixture.h"
#include "linyaps_box/config/oci_config.h"

#include <fstream>

namespace linyaps_box {
using namespace linyaps_box::config;

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::SizeIs;

using linyaps_box::test::load_fixture;
using linyaps_box::test::parse_config;

TEST(ProcessParse, FixtureProcess)
{
    const auto content = load_fixture("data/oci.json");
    const auto config = oci_config::parse(std::string_view{ content });

    ASSERT_TRUE(config.process_.has_value());
    const auto &process = *config.process_;
    EXPECT_TRUE(process.terminal.value_or(false));
    ASSERT_TRUE(process.console_size_.has_value());
    EXPECT_EQ(process.console_size_->height, 24U);
    EXPECT_EQ(process.console_size_->width, 80U);
    EXPECT_THAT(process.cwd.string(), Eq("/work"));
    EXPECT_THAT(process.args, ElementsAre("/bin/echo", "hello"));
    ASSERT_TRUE(process.env.has_value());
    EXPECT_THAT(*process.env, ElementsAre("PATH=/usr/bin", "TERM=xterm"));
    EXPECT_THAT(process.user_.uid, Eq(1000));
    EXPECT_THAT(process.user_.gid, Eq(1000));
    ASSERT_TRUE(process.user_.additional_gids.has_value());
    EXPECT_THAT(*process.user_.additional_gids, ElementsAre(10, 20));
    EXPECT_TRUE(process.no_new_privileges.value_or(false));
    EXPECT_THAT(process.oom_score_adj, Eq(500));

    ASSERT_TRUE(process.rlimits.has_value());
    EXPECT_THAT(*process.rlimits, SizeIs(2));
    EXPECT_EQ((*process.rlimits)[0].type_, rlimit::type::nofile);
    EXPECT_EQ((*process.rlimits)[0].soft, 1024);

    ASSERT_TRUE(process.scheduler_.has_value());
    EXPECT_EQ(process.scheduler_->policy_, scheduler::policy::deadline);
    EXPECT_THAT(process.scheduler_->runtime, Eq(100000));
}

TEST(ProcessParse, MissingCwdRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config("", R"({"args": ["/bin/true"], "user": {"uid": 0, "gid": 0}})"),
                 std::runtime_error);
}

TEST(ProcessParse, MissingArgsRejected)
{
    EXPECT_THROW(std::ignore = parse_config("", R"({"cwd": "/", "user": {"uid": 0, "gid": 0}})"),
                 std::runtime_error);
}

TEST(ProcessParse, EmptyArgsRejected)
{
    EXPECT_THROW(std::ignore =
                   parse_config("", R"({"cwd": "/", "args": [], "user": {"uid": 0, "gid": 0}})"),
                 std::runtime_error);
}

TEST(ProcessParse, DuplicateRlimitsRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "rlimits": [{"type": "RLIMIT_NOFILE", "soft": 1024, "hard": 1024}, {"type": "RLIMIT_NOFILE", "soft": 2048, "hard": 2048}]})"),
      std::runtime_error);
}

TEST(ProcessParse, UmaskHighBitsRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0, "umask": 4095}})"),
      std::runtime_error);
}

TEST(ProcessParse, ConsoleSizeDroppedWithoutTerminal)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "consoleSize": {"height": 24, "width": 80}})");
    ASSERT_TRUE(config.process_.has_value());
    EXPECT_FALSE(config.process_->console_size_.has_value());
}

TEST(ProcessParse, ConsoleSizeKeptWithTerminal)
{
    const auto config = parse_config(
      "",
      R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "terminal": true, "consoleSize": {"height": 30, "width": 100}})");
    ASSERT_TRUE(config.process_.has_value());
    ASSERT_TRUE(config.process_->console_size_.has_value());
    EXPECT_EQ(config.process_->console_size_->height, 30U);
    EXPECT_EQ(config.process_->console_size_->width, 100U);
}

TEST(ProcessParse, UnknownRlimitTypeRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "rlimits": [{"type": "RLIMIT_NOPE", "soft": 1024, "hard": 1024}]})"),
      std::runtime_error);
}

TEST(ProcessParse, RelativeCwdRejected)
{
    EXPECT_THROW(std::ignore = parse_config(
                   "",
                   R"({"cwd": "relative", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}})"),
                 std::runtime_error);
}

TEST(ProcessParse, InvalidEnvRejected)
{
    EXPECT_THROW(
      std::ignore = parse_config(
        "",
        R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}, "env": ["NOEQUALS"]})"),
      std::runtime_error);
}

TEST(ProcessParse, NullUserResetsToDefault)
{
    const auto config = parse_config("", R"({"cwd": "/", "args": ["/bin/true"], "user": null})");
    ASSERT_TRUE(config.process_.has_value());
    EXPECT_EQ(config.process_->user_.uid, 0U);
    EXPECT_EQ(config.process_->user_.gid, 0U);
    EXPECT_FALSE(config.process_->user_.umask.has_value());
}

TEST(ProcessParse, ParseContentSucceeds)
{
    const auto content = R"({"cwd": "/", "args": ["/bin/echo"], "user": {"uid": 1, "gid": 2}})"sv;
    const auto process = process::parse(content);
    EXPECT_THAT(process.cwd.string(), Eq("/"));
    EXPECT_THAT(process.args, ElementsAre("/bin/echo"));
    EXPECT_EQ(process.user_.uid, 1U);
    EXPECT_EQ(process.user_.gid, 2U);
}

TEST(ProcessParse, ParseFromFileSucceeds)
{
    const auto path = std::filesystem::path{ "process_parse_test.json" };
    {
        std::ofstream out{ path };
        out << R"({"cwd": "/", "args": ["/bin/echo"], "user": {"uid": 1, "gid": 2}})";
    }

    const auto process = process::parse(path);
    EXPECT_EQ(process.user_.uid, 1U);
    EXPECT_THAT(process.args, ElementsAre("/bin/echo"));

    std::filesystem::remove(path);
}

TEST(ProcessParse, ParseMissingFileThrows)
{
    EXPECT_THROW(process::parse(std::filesystem::path{ "/nonexistent/process.json" }),
                 std::runtime_error);
}

TEST(ProcessParse, ParseDirectoryThrows)
{
    EXPECT_THROW(process::parse(std::filesystem::path{ "data" }), std::runtime_error);
}

} // namespace
} // namespace linyaps_box
