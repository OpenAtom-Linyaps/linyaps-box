// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <linyaps_box/utils/semver.h>

#include <sstream>
#include <unordered_set>

namespace semver = linyaps_box::utils;

TEST(Semver, ConstructFromParts)
{
    const semver::semver v0(1, 2, 3);
    EXPECT_EQ(v0.major_version(), 1);
    EXPECT_EQ(v0.minor_version(), 2);
    EXPECT_EQ(v0.patch_version(), 3);
    EXPECT_TRUE(v0.prerelease().empty());
    EXPECT_TRUE(v0.build().empty());

    const semver::semver v1(1, 2, 3, "alpha");
    EXPECT_EQ(v1.major_version(), 1);
    EXPECT_EQ(v1.prerelease(), "alpha");
    EXPECT_TRUE(v1.build().empty());

    const semver::semver v2(1, 2, 3, "", "build.123");
    EXPECT_EQ(v2.minor_version(), 2);
    EXPECT_TRUE(v2.prerelease().empty());
    EXPECT_EQ(v2.build(), "build.123");

    const semver::semver v3(1, 2, 3, "rc.1", "sha.abc");
    EXPECT_EQ(v3.prerelease(), "rc.1");
    EXPECT_EQ(v3.build(), "sha.abc");
}

TEST(Semver, ConstructFromString)
{
    const auto v1 = semver::semver("1.2.3");
    EXPECT_EQ(v1.major_version(), 1);
    EXPECT_EQ(v1.prerelease().empty(), true);

    const auto v2 = semver::semver("0.0.0");
    EXPECT_EQ(v2.major_version(), 0);

    const auto v3 = semver::semver("1.2.3-alpha");
    EXPECT_EQ(v3.prerelease(), "alpha");

    const auto v4 = semver::semver("1.2.3+build.42");
    EXPECT_EQ(v4.build(), "build.42");

    const auto v5 = semver::semver("1.2.3-rc.1+build.42");
    EXPECT_EQ(v5.prerelease(), "rc.1");
    EXPECT_EQ(v5.build(), "build.42");

    const auto v6 = semver::semver("1.0.0-alpha.beta.1");
    EXPECT_EQ(v6.prerelease(), "alpha.beta.1");

    const auto v7 = semver::semver("1.2.3+build.alpha.beta");
    EXPECT_EQ(v7.build(), "build.alpha.beta");

    const auto v8 = semver::semver("1.2.3-alpha+build.1.2.3");
    EXPECT_EQ(v8.prerelease(), "alpha");
    EXPECT_EQ(v8.build(), "build.1.2.3");

    const auto v9 = semver::semver("1.2.3-alpha-.-beta");
    EXPECT_EQ(v9.prerelease(), "alpha-.-beta");
}

TEST(Semver, RejectsLeadingZeros)
{
    EXPECT_THROW(semver::semver("01.2.3"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.02.3"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.03"), std::invalid_argument);
}

TEST(Semver, RejectsNegativeNumbers)
{
    EXPECT_THROW(semver::semver("-1.2.3"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.-2.3"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.-3"), std::invalid_argument);
}

TEST(Semver, RejectsMissingSegments)
{
    EXPECT_THROW(semver::semver("1.2"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1"), std::invalid_argument);
    EXPECT_THROW(semver::semver(""), std::invalid_argument);
}

TEST(Semver, RejectsTrailingGarbage)
{
    EXPECT_THROW(semver::semver("1.2.3abc"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3.4"), std::invalid_argument);
}

TEST(Semver, RejectsEmptyTag)
{
    EXPECT_THROW(semver::semver("1.2.3-"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3+"), std::invalid_argument);
}

TEST(Semver, RejectsInvalidTagChars)
{
    EXPECT_THROW(semver::semver("1.2.3-alpha$"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3-alpha space"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3-alpha..1"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3+build$"), std::invalid_argument);
}

TEST(Semver, RejectsNumericLeadingZerosInTags)
{
    EXPECT_THROW(semver::semver("1.2.3-01"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3-alpha.01"), std::invalid_argument);
    EXPECT_THROW(semver::semver("1.2.3+01"), std::invalid_argument);
}

TEST(Semver, RejectsOverflow)
{
    EXPECT_THROW(semver::semver("9999999999.0.0"), std::invalid_argument);
}

TEST(Semver, RejectsInvalidPrefix)
{
    EXPECT_THROW(semver::semver("v1.2.3"), std::invalid_argument);
    EXPECT_THROW(semver::semver(" 1.2.3"), std::invalid_argument);
}

TEST(Semver, ToStringRoundTrip)
{
    auto inputs = { "1.0.0",       "1.2.3-alpha",
                    "1.2.3+build", "1.2.3-rc.1+build.42",
                    "0.0.0",       "10.20.30-alpha.beta.99+sha.abc.123" };
    for (const auto *input : inputs) {
        EXPECT_EQ(semver::semver(input).to_string(), input);
    }
}

TEST(Semver, StreamOutput)
{
    const semver::semver v(1, 2, 3, "rc.1");
    std::ostringstream os;
    os << v;
    EXPECT_EQ(os.str(), "1.2.3-rc.1");
}

TEST(Semver, Equal)
{
    EXPECT_TRUE(semver::semver(1, 2, 3) == semver::semver(1, 2, 3));
    EXPECT_FALSE(semver::semver(1, 2, 3) == semver::semver(2, 2, 3));
    EXPECT_FALSE(semver::semver(1, 2, 3, "alpha") == semver::semver(1, 2, 3));
    EXPECT_TRUE(semver::semver(1, 2, 3, "alpha") == semver::semver(1, 2, 3, "alpha"));
}

TEST(Semver, EqualBuildIgnored)
{
    EXPECT_TRUE(semver::semver(1, 2, 3, "", "a") == semver::semver(1, 2, 3, "", "b"));
    EXPECT_TRUE(semver::semver("1.2.3-beta+build") == semver::semver("1.2.3-beta+otherbuild"));
    EXPECT_TRUE(semver::semver("1.2.3+build") == semver::semver("1.2.3+otherbuild"));
}

TEST(Semver, NotEqual)
{
    EXPECT_TRUE(semver::semver(1, 0, 0) != semver::semver(2, 0, 0));
    EXPECT_FALSE(semver::semver(1, 0, 0) != semver::semver(1, 0, 0));
}

TEST(Semver, PrecedencePrereleaseSpecExamples)
{
    // From semver 2.0.0
    EXPECT_TRUE(semver::semver("1.0.0-alpha") < semver::semver("1.0.0-alpha.1"));
    EXPECT_TRUE(semver::semver("1.0.0-alpha.1") < semver::semver("1.0.0-alpha.beta"));
    EXPECT_TRUE(semver::semver("1.0.0-alpha.beta") < semver::semver("1.0.0-beta"));
    EXPECT_TRUE(semver::semver("1.0.0-beta") < semver::semver("1.0.0-beta.2"));
    EXPECT_TRUE(semver::semver("1.0.0-beta.2") < semver::semver("1.0.0-beta.11"));
    EXPECT_TRUE(semver::semver("1.0.0-beta.11") < semver::semver("1.0.0-rc.1"));
    EXPECT_TRUE(semver::semver("1.0.0-rc.1") < semver::semver("1.0.0"));
    EXPECT_TRUE(semver::semver("0.0.0") > semver::semver("0.0.0-foo"));
}

TEST(Semver, PrecedencePrereleaseComparisons)
{
    // All cases

    // Numeric ordering
    EXPECT_TRUE(semver::semver("1.0.0-1") < semver::semver("1.0.0-alpha"));
    EXPECT_TRUE(semver::semver("1.2.3-5") > semver::semver("1.2.3-4"));
    EXPECT_TRUE(semver::semver("1.2.3-a.10") > semver::semver("1.2.3-a.5"));

    // More identifiers > fewer (when preceding equal)
    EXPECT_TRUE(semver::semver("1.2.3-5-foo") > semver::semver("1.2.3-5"));
    EXPECT_TRUE(semver::semver("1.2.3-a.b") > semver::semver("1.2.3-a"));

    // String > numeric in same position
    EXPECT_TRUE(semver::semver("1.2.3-a.b") > semver::semver("1.2.3-a.5"));

    // Lexicographic (not length-aware)
    EXPECT_TRUE(semver::semver("1.2.3-r2") > semver::semver("1.2.3-r100"));

    // ASCII case sensitivity
    EXPECT_TRUE(semver::semver("1.2.3-5-foo") > semver::semver("1.2.3-5-Foo"));
    EXPECT_TRUE(semver::semver("1.2.3-r100") > semver::semver("1.2.3-R2"));

    // Complex: first differing numeric wins, rest ignored
    EXPECT_TRUE(semver::semver("1.2.3-a.b.c.10.d.5") > semver::semver("1.2.3-a.b.c.5.d.100"));

    // Build metadata does not affect order
    EXPECT_FALSE(semver::semver("1.0.0+build1") < semver::semver("1.0.0+build2"));
    EXPECT_FALSE(semver::semver("1.0.0+build2") < semver::semver("1.0.0+build1"));
}

TEST(Semver, RelationalOperators)
{
    const auto low = semver::semver("1.0.0");
    const auto high = semver::semver("2.0.0");

    EXPECT_TRUE(low < high);
    EXPECT_TRUE(low <= high);
    EXPECT_TRUE(low <= low);
    EXPECT_TRUE(high > low);
    EXPECT_TRUE(high >= low);
    EXPECT_TRUE(high >= high);
    EXPECT_FALSE(low > high);
    EXPECT_FALSE(high < low);

    // Numeric precedence at minor/patch level
    EXPECT_TRUE(semver::semver(1, 0, 0) < semver::semver(1, 1, 0));
    EXPECT_TRUE(semver::semver(1, 0, 0) < semver::semver(1, 0, 1));
}

TEST(Semver, Compatible)
{
    EXPECT_TRUE(semver::semver("1.0.0").is_compatible_with(semver::semver("1.0.0")));
    EXPECT_TRUE(semver::semver("1.5.0").is_compatible_with(semver::semver("1.0.0")));
    EXPECT_FALSE(semver::semver("1.0.0").is_compatible_with(semver::semver("1.5.0")));
    EXPECT_FALSE(semver::semver("2.0.0").is_compatible_with(semver::semver("1.0.0")));
    EXPECT_FALSE(semver::semver("1.0.0").is_compatible_with(semver::semver("2.0.0")));
    EXPECT_TRUE(semver::semver("1.0.0").is_compatible_with(semver::semver("1.0.0-alpha")));
    EXPECT_FALSE(semver::semver("1.0.0-alpha").is_compatible_with(semver::semver("1.0.0")));
}

TEST(Semver, Hash)
{
    std::unordered_set<semver::semver> set;
    set.insert(semver::semver("1.0.0"));
    set.insert(semver::semver("1.0.0"));
    EXPECT_EQ(set.size(), 1);

    set.clear();
    set.insert(semver::semver("1.0.0+build1"));
    set.insert(semver::semver("1.0.0+build2"));
    EXPECT_EQ(set.size(), 1);

    set.clear();
    set.insert(semver::semver("1.0.0"));
    set.insert(semver::semver("1.0.0-alpha"));
    EXPECT_EQ(set.size(), 2);
}

TEST(Semver, LargeOrLongInputs)
{
    const auto v1 = semver::semver("999999.999999.999999");
    EXPECT_EQ(v1.major_version(), 999999);

    const auto v2 = semver::semver("1.0.0-a.b.c.d.e.f");
    EXPECT_EQ(v2.prerelease(), "a.b.c.d.e.f");
}
