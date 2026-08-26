// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/infra/process_handle.h"

#include <fmt/format.h>

#include <cstdint>
#include <string>

namespace infra = linyaps_box::infra;

namespace {

// /proc/<pid>/stat fields 1..21 followed by starttime (field 22).
auto make_stat_line(char state, std::uint64_t starttime) -> std::string
{
    return fmt::format("42 (bash) {} 1 42 42 34816 0 4194304 0 0 0 100 0 0 0 20 0 1 0 0 {}",
                       state,
                       starttime);
}

auto make_stat_line(std::string_view comm, char state, std::uint64_t starttime) -> std::string
{
    return fmt::format("42 ({}) {} 1 42 42 34816 0 4194304 0 0 0 100 0 0 0 20 0 1 0 0 {}",
                       comm,
                       state,
                       starttime);
}

} // namespace

TEST(ParseProcStat, Basic)
{
    auto result = infra::parse_proc_stat(make_stat_line('S', 987654));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, infra::process_state::sleeping);
    EXPECT_EQ(result->start_time, 987654);
}

TEST(ParseProcStat, AllStates)
{
    struct state_case
    {
        char c;
        infra::process_state expected;
    };

    const std::array<state_case, 9> cases{ {
      { 'R', infra::process_state::running },
      { 'S', infra::process_state::sleeping },
      { 'D', infra::process_state::disk_sleep },
      { 'T', infra::process_state::stopped },
      { 't', infra::process_state::tracing_stop },
      { 'X', infra::process_state::dead },
      { 'Z', infra::process_state::zombie },
      { 'P', infra::process_state::parked },
      { 'I', infra::process_state::idle },
    } };

    for (const auto &c : cases) {
        auto result = infra::parse_proc_stat(make_stat_line(c.c, 1));
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->state, c.expected);
    }
}

TEST(ParseProcStat, UnknownStateChar)
{
    auto result = infra::parse_proc_stat(make_stat_line('?', 1));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, infra::process_state::unknown);
}

TEST(ParseProcStat, CommWithSpacesAndParens)
{
    auto result = infra::parse_proc_stat(make_stat_line("my process (with) parens", 'R', 1234));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, infra::process_state::running);
    EXPECT_EQ(result->start_time, 1234);
}

TEST(ParseProcStat, TrailingNewline)
{
    auto result = infra::parse_proc_stat(make_stat_line('S', 42) + "\n");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->state, infra::process_state::sleeping);
    EXPECT_EQ(result->start_time, 42);
}

TEST(ParseProcStat, LargeStartTime)
{
    constexpr auto large_time = std::numeric_limits<uint64_t>::max();
    auto result = infra::parse_proc_stat(make_stat_line('R', large_time));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->start_time, large_time);
}

TEST(ParseProcStat, MissingClosingParen)
{
    auto result =
      infra::parse_proc_stat("42 bash S 1 42 42 34816 0 0 0 0 0 0 100 0 0 0 20 0 1 0 0 5");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST(ParseProcStat, EmptyLine)
{
    auto result = infra::parse_proc_stat("");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST(ParseProcStat, CommOnly)
{
    auto result = infra::parse_proc_stat("42 (bash)");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST(ParseProcStat, NonNumericStartTime)
{
    auto result =
      infra::parse_proc_stat("42 (bash) S 1 42 42 34816 0 4194304 0 0 0 100 0 0 0 20 0 1 0 0 abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}

TEST(ParseProcStat, TooFewFields)
{
    auto result = infra::parse_proc_stat("42 (bash) S 1 42 42");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
}
