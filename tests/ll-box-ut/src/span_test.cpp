// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <linyaps_box/utils/span.h>

#include <array>
#include <vector>

namespace utils = linyaps_box::utils;

TEST(SpanAt, ValidIndexDynamicExtent)
{
    std::array arr{ 10, 20, 30, 40 };
    const utils::span<int> s(arr);

    EXPECT_EQ(s.at(0), 10);
    EXPECT_EQ(s.at(1), 20);
    EXPECT_EQ(s.at(2), 30);
    EXPECT_EQ(s.at(3), 40);
}

TEST(SpanAt, OutOfRangeThrows)
{
    std::array arr{ 1, 2, 3 };
    const utils::span<int> s(arr);

    EXPECT_THROW(std::ignore = s.at(3), std::out_of_range);
    EXPECT_THROW(std::ignore = s.at(100), std::out_of_range);
}

TEST(SpanAt, EmptySpanThrows)
{
    const utils::span<int> s;
    EXPECT_THROW(std::ignore = s.at(0), std::out_of_range);
}

TEST(SpanAt, ReturnsMutableReference)
{
    std::array arr{ 1, 2 };
    const utils::span<int> s(arr);

    s.at(1) = 42;
    EXPECT_EQ(arr[1], 42);
}

TEST(SpanAt, ConstSpanReadsCorrectly)
{
    std::array arr{ 5, 6 };
    const utils::span<const int> s(arr);

    EXPECT_EQ(s.at(0), 5);
    EXPECT_EQ(s.at(1), 6);
}

TEST(SpanAt, BoundaryIndex)
{
    std::array arr = { 99 };
    const utils::span<int> s(arr);

    EXPECT_EQ(s.at(0), 99);
    EXPECT_THROW(std::ignore = s.at(1), std::out_of_range);
}

TEST(SpanAt, SubspanAt)
{
    std::array raw{ 0, 1, 2, 3, 4 };
    auto s = utils::span<int>(raw).subspan(1, 3);

    EXPECT_EQ(s.at(0), 1);
    EXPECT_EQ(s.at(2), 3);
    EXPECT_THROW(std::ignore = s.at(3), std::out_of_range);
}
