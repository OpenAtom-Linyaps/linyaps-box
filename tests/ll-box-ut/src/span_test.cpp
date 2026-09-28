// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <linyaps_box/utils/span.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <numeric>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace utils = linyaps_box::utils;

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::SizeIs;

// gtest >= 1.10 renamed TYPED_TEST_CASE to TYPED_TEST_SUITE. The build only
// shims INSTANTIATE_TEST_SUITE_P, so keep a local shim to stay 1.8.1 friendly.
// The trailing comma passes an explicit (empty) name-generator pack: newer
// TYPED_TEST_SUITE is variadic and omitting it is a post-C++17 extension.
#ifdef TYPED_TEST_SUITE
#  define SPAN_TYPED_TEST_SUITE(Suite, Types) TYPED_TEST_SUITE(Suite, Types, )
#else
#  define SPAN_TYPED_TEST_SUITE(Suite, Types) TYPED_TEST_CASE(Suite, Types)
#endif

namespace {

using dyn_span = utils::span<int>;
using fixed_span = utils::span<int, 5>;

static_assert(sizeof(dyn_span) == 2 * sizeof(void *), "dynamic span is pointer + size");
static_assert(sizeof(fixed_span) == sizeof(void *), "fixed span stores only the pointer");
static_assert(std::is_trivially_copyable_v<dyn_span>, "span should be trivially copyable");

static_assert(std::is_same_v<dyn_span::value_type, int>);
static_assert(std::is_same_v<utils::span<const int>::element_type, const int>);
static_assert(dyn_span::extent == utils::dynamic_extent);
static_assert(fixed_span::extent == 5);

// Default construction is only allowed for dynamic or zero extent.
static_assert(std::is_default_constructible_v<dyn_span>);
static_assert(std::is_default_constructible_v<utils::span<int, 0>>);
static_assert(!std::is_default_constructible_v<fixed_span>);

// Deduction guides preserve the extent (and constness) of the source range.
static_assert(
  std::is_same_v<decltype(utils::span(std::declval<int (&)[5]>())), utils::span<int, 5>>);
static_assert(
  std::is_same_v<decltype(utils::span(std::declval<std::array<int, 5> &>())), utils::span<int, 5>>);
static_assert(std::is_same_v<decltype(utils::span(std::declval<const std::array<int, 5> &>())),
                             utils::span<const int, 5>>);
static_assert(std::is_same_v<decltype(utils::span(std::declval<const int (&)[3]>())),
                             utils::span<const int, 3>>);

// P1976R2: fixed-size span construction from a dynamic-size range is explicit.
static_assert(std::is_convertible_v<std::vector<int> &, dyn_span>);
static_assert(!std::is_convertible_v<std::vector<int> &, utils::span<int, 3>>);
static_assert(std::is_convertible_v<utils::span<int, 5> &, utils::span<const int, 5>>);
static_assert(!std::is_convertible_v<utils::span<int> &, utils::span<const int, 5>>);

static_assert(utils::is_span_v<dyn_span>);
static_assert(utils::is_span_v<const utils::span<int, 3> &>);
static_assert(!utils::is_span_v<std::vector<int>>);

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

static_assert(has_as_bytes<int>::value);
static_assert(has_as_bytes<const int>::value);
static_assert(!has_as_bytes<volatile int>::value);
static_assert(has_as_writable_bytes<int>::value);
static_assert(!has_as_writable_bytes<const int>::value);
static_assert(!has_as_writable_bytes<volatile int>::value);

constexpr std::array<int, 4> const_array{ 1, 2, 3, 4 };
constexpr utils::span<const int, 4> const_span(const_array);
static_assert(const_span.size() == 4);
static_assert(const_span.size_bytes() == 4 * sizeof(int));
static_assert(const_span.front() == 1);
static_assert(const_span.back() == 4);
static_assert(const_span[2] == 3);
static_assert(!const_span.empty());
static_assert(const_span.first<2>()[1] == 2);
static_assert(const_span.last<2>()[0] == 3);
static_assert(const_span.subspan<1, 2>()[0] == 2);

} // namespace

template <typename SpanType>
class SpanViewTest : public ::testing::Test
{
protected:
    std::array<int, 5> data{ 1, 2, 3, 4, 5 };

    [[nodiscard]] auto view() -> SpanType { return SpanType(data); }
};

using SpanExtents = ::testing::Types<utils::span<int>, utils::span<int, 5>>;
SPAN_TYPED_TEST_SUITE(SpanViewTest, SpanExtents);

TYPED_TEST(SpanViewTest, Observers)
{
    const auto s = this->view();
    EXPECT_THAT(s, SizeIs(5));
    EXPECT_THAT(s, Not(IsEmpty()));
    EXPECT_EQ(s.data(), this->data.data());
    EXPECT_EQ(s.size_bytes(), 5 * sizeof(int));
}

TYPED_TEST(SpanViewTest, ElementsAndIndexing)
{
    auto s = this->view();
    EXPECT_THAT(s, ElementsAre(1, 2, 3, 4, 5));
    EXPECT_THAT(s.data(), Pointee(Eq(1)));
    EXPECT_EQ(s.front(), 1);
    EXPECT_EQ(s.back(), 5);
    EXPECT_EQ(s[2], 3);
}

TYPED_TEST(SpanViewTest, MutationWritesThrough)
{
    auto s = this->view();
    s[0] = 10;
    s.front() = 11;
    s.at(2) = 30;
    s.back() = 50;
    EXPECT_THAT(this->data, ElementsAre(11, 2, 30, 4, 50));
}

TYPED_TEST(SpanViewTest, ForwardIteration)
{
    auto s = this->view();
    EXPECT_EQ(std::accumulate(s.cbegin(), s.cend(), 0), 15);
    EXPECT_EQ(std::distance(s.begin(), s.end()), 5);
    *s.begin() = 42;
    EXPECT_EQ(this->data[0], 42);
}

TYPED_TEST(SpanViewTest, ReverseIteration)
{
    auto s = this->view();
    EXPECT_THAT(std::vector<int>(s.rbegin(), s.rend()), ElementsAre(5, 4, 3, 2, 1));
    *s.rbegin() = 7;
    EXPECT_EQ(this->data.back(), 7);
}

TYPED_TEST(SpanViewTest, RuntimeSubviews)
{
    const auto s = this->view();
    EXPECT_THAT(s.first(2), ElementsAre(1, 2));
    EXPECT_THAT(s.last(2), ElementsAre(4, 5));
    EXPECT_THAT(s.subspan(1, 2), ElementsAre(2, 3));
    EXPECT_THAT(s.subspan(2), ElementsAre(3, 4, 5));
    EXPECT_THAT(s.first(0), IsEmpty());
    EXPECT_THAT(s.last(0), IsEmpty());
    EXPECT_THAT(s.subspan(5), IsEmpty());
}

TYPED_TEST(SpanViewTest, ByteViews)
{
    auto s = this->view();
    const auto bytes = utils::as_bytes(s);
    EXPECT_THAT(bytes, SizeIs(5 * sizeof(int)));
    EXPECT_EQ(bytes.data(), reinterpret_cast<const std::byte *>(s.data()));
    if constexpr (TypeParam::extent != utils::dynamic_extent) {
        static_assert(std::remove_cv_t<decltype(bytes)>::extent == 5 * sizeof(int));
    }

    auto writable = utils::as_writable_bytes(s);
    ASSERT_THAT(writable, SizeIs(5 * sizeof(int)));
    std::memset(writable.data(), 0, writable.size());
    EXPECT_THAT(this->data, ElementsAre(0, 0, 0, 0, 0));
}

TEST(SpanDefault, DynamicSpanIsEmpty)
{
    const utils::span<int> s;
    EXPECT_THAT(s, IsEmpty());
    EXPECT_EQ(s.data(), nullptr);
    EXPECT_EQ(s.size(), 0U);
    EXPECT_EQ(s.begin(), s.end());
    EXPECT_EQ(s.rbegin(), s.rend());
}

TEST(SpanDefault, ZeroExtentIsDefaultConstructible)
{
    const utils::span<int, 0> s;
    EXPECT_THAT(s, IsEmpty());
    EXPECT_EQ(s.data(), nullptr);
}

class SpanAtTest : public ::testing::Test
{
protected:
    std::array<int, 4> data{ 10, 20, 30, 40 };
    utils::span<int> span{ data };
};

TEST_F(SpanAtTest, ReturnsElement)
{
    EXPECT_EQ(span.at(0), 10);
    EXPECT_EQ(span.at(3), 40);
}

TEST_F(SpanAtTest, YieldsMutableReference)
{
    span.at(1) = 21;
    EXPECT_THAT(data, ElementsAre(10, 21, 30, 40));
}

TEST_F(SpanAtTest, ThrowsWhenOutOfRange)
{
    EXPECT_THROW(std::ignore = span.at(4), std::out_of_range);
}

TEST(SpanAt, EmptySpanThrows)
{
    const utils::span<int> s;
    EXPECT_THROW(std::ignore = s.at(0), std::out_of_range);
}

namespace {

struct SpanSources
{
    std::vector<int> vec{ 1, 2, 3 };
    std::array<int, 3> arr{ 1, 2, 3 };
    int c_arr[3]{ 1, 2, 3 }; // NOLINT
};

struct SpanSourceCase
{
    const char *name;
    utils::span<const int> (*make)(const SpanSources &);
};

auto from_vector(const SpanSources &sources) -> utils::span<const int>
{
    return { sources.vec };
}

auto from_std_array(const SpanSources &sources) -> utils::span<const int>
{
    return { sources.arr };
}

auto from_c_array(const SpanSources &sources) -> utils::span<const int>
{
    return { sources.c_arr };
}

auto from_iterators(const SpanSources &sources) -> utils::span<const int>
{
    return { sources.vec.cbegin(), sources.vec.cend() };
}

auto from_pointer_count(const SpanSources &sources) -> utils::span<const int>
{
    return { sources.vec.data(), sources.vec.size() };
}

void PrintTo(const SpanSourceCase &value, std::ostream *os) // NOLINT
{
    *os << value.name;
}

} // namespace

class SpanSourceTest : public ::testing::TestWithParam<SpanSourceCase>
{
protected:
    SpanSources sources;
};

TEST_P(SpanSourceTest, YieldsSameView)
{
    const auto s = GetParam().make(sources);
    EXPECT_THAT(s, ElementsAre(1, 2, 3));
    EXPECT_EQ(s.size(), 3U);
}

INSTANTIATE_TEST_SUITE_P(From,
                         SpanSourceTest,
                         ::testing::Values(SpanSourceCase{ "vector", &from_vector },
                                           SpanSourceCase{ "std::array", &from_std_array },
                                           SpanSourceCase{ "c-array", &from_c_array },
                                           SpanSourceCase{ "iterators", &from_iterators },
                                           SpanSourceCase{ "pointer+count", &from_pointer_count }));

TEST(SpanConstruction, FixedExtentFromContainerIsExplicit)
{
    const std::vector<int> v{ 1, 2, 3 };
    const utils::span<const int, 3> s(v);
    static_assert(decltype(s)::extent == 3);
    EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(SpanConstruction, IteratorCount)
{
    const std::vector<int> v{ 1, 2, 3, 4 };
    const utils::span<const int> s(v.cbegin(), 2);
    EXPECT_THAT(s, ElementsAre(1, 2));
}

TEST(SpanConstruction, IteratorCountFixedExtentIsExplicit)
{
    const std::vector<int> v{ 1, 2, 3 };
    const utils::span<const int, 3> s(v.cbegin(), 3);
    EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(SpanConstruction, FromMutableStringWritesThrough)
{
    std::string text = "hello";
    const utils::span<char> s(text);
    ASSERT_EQ(s.size(), text.size());
    s.front() = 'H';
    EXPECT_EQ(text, "Hello");
}

TEST(SpanConstruction, FromConstStringYieldsConstElements)
{
    const std::string text = "abc";
    const utils::span s(text);
    static_assert(std::is_same_v<decltype(s)::element_type, const char>);
    EXPECT_THAT(s, ElementsAre('a', 'b', 'c'));
}

TEST(SpanDeduction, PreservesExtentOfCAndStdArray)
{
    int carr[3]{ 1, 2, 3 }; // NOLINT
    std::array<int, 2> arr{ 4, 5 };
    const utils::span from_c(carr);
    const utils::span from_a(arr);
    static_assert(decltype(from_c)::extent == 3);
    static_assert(decltype(from_a)::extent == 2);
    EXPECT_THAT(from_c, ElementsAre(1, 2, 3));
    EXPECT_THAT(from_a, ElementsAre(4, 5));
}

TEST(SpanConversion, MutableToConstSharesBackingStore)
{
    std::array<int, 3> a{ 1, 2, 3 };
    const utils::span<int> mut(a);
    const utils::span<const int> cst(mut);
    EXPECT_EQ(cst.data(), mut.data());
    EXPECT_THAT(cst, ElementsAre(1, 2, 3));
}

TEST(SpanConversion, AssignmentFromConvertibleSpan)
{
    std::array<int, 3> a{ 4, 5, 6 };
    const utils::span<int> mut(a);
    utils::span<const int> cst;
    cst = mut;
    EXPECT_THAT(cst, ElementsAre(4, 5, 6));
}

TEST(SpanConversion, DynamicToFixedIsExplicit)
{
    std::array<int, 3> a{ 1, 2, 3 };
    const utils::span<const int> dyn(a);
    const utils::span<const int, 3> fixed(dyn);
    EXPECT_THAT(fixed, ElementsAre(1, 2, 3));
}

TEST(SpanCopy, CopyConstructorAndAssignment)
{
    std::array<int, 3> a{ 1, 2, 3 };
    const utils::span<int> s1(a);
    const utils::span<int> s2(s1);
    utils::span<int> s3;
    s3 = s1;
    EXPECT_EQ(s2.data(), s1.data());
    EXPECT_EQ(s3.data(), s1.data());
    EXPECT_THAT(s2, ElementsAre(1, 2, 3));
    EXPECT_THAT(s3, ElementsAre(1, 2, 3));
}

TEST(SpanSubviews, TemplateOnFixedExtent)
{
    std::array<int, 5> a{ 1, 2, 3, 4, 5 };
    const utils::span s(a);
    const auto first = s.first<2>();
    const auto last = s.last<2>();
    const auto middle = s.subspan<1, 3>();
    static_assert(decltype(s)::extent == 5);
    static_assert(decltype(first)::extent == 2);
    static_assert(decltype(last)::extent == 2);
    static_assert(decltype(middle)::extent == 3);
    EXPECT_THAT(first, ElementsAre(1, 2));
    EXPECT_THAT(last, ElementsAre(4, 5));
    EXPECT_THAT(middle, ElementsAre(2, 3, 4));
}

TEST(SpanSubviews, TemplateOnDynamicExtent)
{
    std::array<int, 5> a{ 1, 2, 3, 4, 5 };
    const utils::span<int> s(a);
    const auto first = s.first<2>();
    const auto last = s.last<2>();
    const auto middle = s.subspan<1, 3>();
    static_assert(decltype(s)::extent == utils::dynamic_extent);
    static_assert(decltype(first)::extent == 2);
    static_assert(decltype(last)::extent == 2);
    static_assert(decltype(middle)::extent == 3);
    EXPECT_THAT(first, ElementsAre(1, 2));
    EXPECT_THAT(last, ElementsAre(4, 5));
    EXPECT_THAT(middle, ElementsAre(2, 3, 4));
}

TEST(SpanSubviews, DefaultCountKeepsRemainingExtent)
{
    std::array<int, 5> a{ 1, 2, 3, 4, 5 };
    const utils::span s(a);
    const auto rest = s.subspan<1>();
    static_assert(decltype(rest)::extent == 4);
    EXPECT_THAT(rest, ElementsAre(2, 3, 4, 5));
}

TEST(SpanSubviews, ChainedRuntimeSubspans)
{
    std::array<int, 6> a{ 0, 1, 2, 3, 4, 5 };
    const utils::span s(a);
    EXPECT_THAT(s.subspan(1).subspan(1, 3), ElementsAre(2, 3, 4));
}

TEST(SpanBytes, EmptySpanYieldsEmptyByteView)
{
    const utils::span<const int> s;
    const auto bytes = utils::as_bytes(s);
    EXPECT_THAT(bytes, IsEmpty());
    EXPECT_EQ(bytes.data(), nullptr);
}

TEST(SpanBytes, CharSpanRoundTripsThroughBytes)
{
    std::array<char, 4> raw{ 'a', 'b', 'c', 'd' };
    const auto s = utils::span(raw);
    const auto bytes = utils::as_writable_bytes(s);
    static_assert(decltype(bytes)::extent == 4U);
    EXPECT_EQ(static_cast<char>(bytes[0]), 'a');
    bytes[0] = std::byte{ 'z' };
    EXPECT_EQ(raw[0], 'z');
}
