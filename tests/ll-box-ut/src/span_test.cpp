// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <linyaps_box/utils/span.h>

#include <array>
#include <cstring>
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

TEST(Subspan, Offset)
{
    std::array raw{ 0, 1, 2, 3, 4 };
    auto s = utils::span<int>(raw).subspan(1, 3);

    EXPECT_EQ(s.at(0), 1);
    EXPECT_EQ(s.at(2), 3);
    EXPECT_THROW(std::ignore = s.at(3), std::out_of_range);
}

// P1976R2: fixed-size span construction from dynamic-size range should be explicit.
// Single-argument constructors are tested via is_convertible_v (which requires implicit
// conversion). Two-argument constructors (ptr,count) and (first,last) are tested via runtime
// positive tests.
static_assert(std::is_convertible_v<std::vector<int> &, utils::span<int>>,
              "dynamic span from container should be implicit");
static_assert(!std::is_convertible_v<std::vector<int> &, utils::span<int, 3>>,
              "fixed span from container should be explicit");

static_assert(std::is_convertible_v<utils::span<int> &, utils::span<const int>>,
              "dynamic→dynamic span should be implicit");
static_assert(!std::is_convertible_v<utils::span<int> &, utils::span<const int, 5>>,
              "dynamic→fixed span should be explicit");
static_assert(std::is_convertible_v<utils::span<int, 5> &, utils::span<const int, 5>>,
              "fixed→fixed (matching) span should be implicit");
static_assert(std::is_convertible_v<utils::span<int, 5> &, utils::span<const int>>,
              "fixed→dynamic span should be implicit");

static_assert(std::is_convertible_v<int (&)[5], utils::span<int>>,
              "dynamic span from c-array should be implicit");
static_assert(std::is_convertible_v<int (&)[5], utils::span<int, 5>>,
              "fixed span from c-array (matching extent) should be implicit");
static_assert(std::is_convertible_v<std::array<int, 5> &, utils::span<int, 5>>,
              "fixed span from std::array (matching extent) should be implicit");
static_assert(std::is_convertible_v<const std::array<int, 5> &, utils::span<const int, 5>>,
              "fixed span from const std::array (matching extent) should be implicit");

static_assert(std::is_default_constructible_v<utils::span<int>>,
              "dynamic span should be default-constructible");
static_assert(!std::is_default_constructible_v<utils::span<int, 5>>,
              "fixed non-zero span should NOT be default-constructible");

TEST(SpanConstructorExplicit, PtrCountDynamic)
{
    const std::vector<int> data{ 1, 2, 3 };
    const utils::span<const int> s(data.data(), 3);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[2], 3);
}

TEST(SpanConstructorExplicit, FirstLastDynamic)
{
    const std::vector<int> data{ 1, 2, 3, 4 };
    const utils::span<const int> s(data.cbegin(), data.cend());
    EXPECT_EQ(s.size(), 4);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[3], 4);
}

TEST(SpanConstructorExplicit, ContainerFixedExplicit)
{
    const std::vector<int> v{ 1, 2, 3 };
    auto s = utils::span<const int, 3>(v);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[2], 3);
}

TEST(SpanConstructorExplicit, ConvertingSpanFixedToDynamic)
{
    std::array arr{ 1, 2, 3 };
    const utils::span<const int, 3> fixed(arr);
    const utils::span<const int> dyn(fixed);
    EXPECT_EQ(dyn.size(), 3);
    EXPECT_EQ(dyn[0], 1);
}

TEST(SpanConstructorExplicit, ConvertingSpanDynamicToFixed)
{
    std::vector<int> arr{ 1, 2, 3 };
    const utils::span<const int> dyn(arr);
    auto fixed = utils::span<const int, 3>(dyn);
    EXPECT_EQ(fixed.size(), 3);
    EXPECT_EQ(fixed[0], 1);
}

TEST(SpanConstructorExplicit, ItCountDynamic)
{
    const std::vector<int> data{ 10, 20, 30 };
    const utils::span<const int> s(data.cbegin(), 2);
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s[0], 10);
    EXPECT_EQ(s[1], 20);
}

TEST(SpanConstructorExplicit, ItEndFixedExplicit)
{
    const std::vector<int> data{ 1, 2, 3 };
    const utils::span<const int, 3> s(data.cbegin(), data.cend());
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[2], 3);
}

TEST(SpanConstructorExplicit, ItCountFixedExplicit)
{
    const std::vector<int> data{ 5, 6, 7 };
    const utils::span<const int, 3> s(data.cbegin(), 3);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 5);
    EXPECT_EQ(s[2], 7);
}

TEST(SpanCTAD, CArray)
{
    int arr[] = { 1, 2, 3 }; // NOLINT
    const utils::span s(arr);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[2], 3);
    static_assert(decltype(s)::extent == 3);
}

TEST(SpanCTAD, StdArray)
{
    std::array arr{ 10, 20 };
    const utils::span s(arr);
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s[0], 10);
    static_assert(decltype(s)::extent == 2);
}

TEST(SpanCTAD, ConstStdArray)
{
    const std::array arr{ 1, 2, 3, 4 };
    const utils::span s(arr);
    EXPECT_EQ(s.size(), 4);
    EXPECT_EQ(s[3], 4);
    static_assert(decltype(s)::extent == 4);
}

TEST(SpanCTAD, ItCount)
{
    std::vector<int> v{ 5, 5, 5 };
    const utils::span s(v.begin(), 2);
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s[0], 5);
    static_assert(decltype(s)::extent == utils::dynamic_extent);
}

TEST(SpanCTAD, ItEnd)
{
    const std::vector<int> v{ 7, 8, 9 };
    const utils::span s(v.cbegin(), v.cend());
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[2], 9);
    static_assert(decltype(s)::extent == utils::dynamic_extent);
}

namespace {

template <typename T, typename = void>
struct has_as_bytes : std::false_type
{
};

template <typename T>
struct has_as_bytes<T, std::void_t<decltype(utils::as_bytes(std::declval<utils::span<T>>()))>>
    : std::true_type
{
};

template <typename T, typename = void>
struct has_as_writable_bytes : std::false_type
{
};

template <typename T>
struct has_as_writable_bytes<
  T,
  std::void_t<decltype(utils::as_writable_bytes(std::declval<utils::span<T>>()))>> : std::true_type
{
};

} // anonymous namespace

static_assert(has_as_bytes<int>::value, "as_bytes should accept int");
static_assert(has_as_bytes<const int>::value, "as_bytes should accept const int");
static_assert(!has_as_bytes<volatile int>::value, "as_bytes should reject volatile");
static_assert(has_as_writable_bytes<int>::value, "as_writable_bytes should accept int");
static_assert(!has_as_writable_bytes<const int>::value, "as_writable_bytes should reject const");
static_assert(!has_as_writable_bytes<volatile int>::value,
              "as_writable_bytes should reject volatile");

TEST(SpanBytes, AsBytes)
{
    const std::array arr{ 0x01020304, 0x05060708 };
    const utils::span s(arr);
    auto bytes = utils::as_bytes(s);
    EXPECT_EQ(bytes.size(), sizeof(arr));
    EXPECT_EQ(bytes.data(), reinterpret_cast<const std::byte *>(s.data()));
}

TEST(SpanBytes, AsWritableBytes)
{
    std::array arr{ 0x01020304 };
    const utils::span s(arr);
    auto bytes = utils::as_writable_bytes(s);
    EXPECT_EQ(bytes.size(), sizeof(int));
    memset(bytes.data(), 0, sizeof(int));
    EXPECT_EQ(arr[0], 0);
    bytes[0] = std::byte(1);
    EXPECT_EQ(arr[0], 1);
}
