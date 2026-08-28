// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/os/net.h"
#include "linyaps_box/utils/enum_traits.h"

#include <cstdint>

namespace os = linyaps_box::os;

namespace {

// The bitmask sentinel must stay the largest flag bit on every Linux
// architecture. On sw_64 SOCK_NONBLOCK is higher than SOCK_CLOEXEC, so pinning
// the sentinel to SOCK_CLOEXEC would reject the enum table at compile time.
TEST(EnumTraits, SocketFlagBitmaskCoversAllFlags)
{
    using flag = os::sys::socket_flag;
    constexpr auto mask = linyaps_box::utils::bitmask_mask<flag>();

    EXPECT_EQ(mask & static_cast<std::uint32_t>(flag::none), 0U);
    EXPECT_NE(mask & static_cast<std::uint32_t>(flag::nonblock), 0U);
    EXPECT_NE(mask & static_cast<std::uint32_t>(flag::cloexec), 0U);

    const auto all = ~flag::none;
    EXPECT_NE(all & flag::nonblock, flag::none);
    EXPECT_NE(all & flag::cloexec, flag::none);
}

TEST(EnumTraits, SocketFlagTableRoundTrip)
{
    using flag = os::sys::socket_flag;
    const auto table = os::sys::get_enum_table(static_cast<flag *>(nullptr));

    const auto nonblock = table.to_name(flag::nonblock);
    ASSERT_TRUE(nonblock.has_value());
    EXPECT_EQ(*nonblock, "SOCK_NONBLOCK");

    const auto cloexec = table.to_name(flag::cloexec);
    ASSERT_TRUE(cloexec.has_value());
    EXPECT_EQ(*cloexec, "SOCK_CLOEXEC");

    const auto parsed = table.from_name("SOCK_NONBLOCK");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, flag::nonblock);
}

} // namespace
