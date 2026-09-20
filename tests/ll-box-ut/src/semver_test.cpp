// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <linyaps_box/utils/semver.h>

#include <limits>
#include <sstream>
#include <string>
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

TEST(Semver, ToStringRoundTrip)
{
    for (const auto *input : { "1.0.0",
                               "1.2.3-alpha",
                               "1.2.3+build",
                               "1.2.3-rc.1+build.42",
                               "0.0.0",
                               "10.20.30-alpha.beta.99+sha.abc.123" }) {
        EXPECT_EQ(semver::semver(input).to_string(), input);
    }
}

TEST(Semver, StreamOutput)
{
    std::ostringstream os;
    os << semver::semver(1, 2, 3, "rc.1");
    EXPECT_EQ(os.str(), "1.2.3-rc.1");
}

TEST(Semver, Equality)
{
    EXPECT_TRUE(semver::semver(1, 2, 3) == semver::semver(1, 2, 3));
    EXPECT_FALSE(semver::semver(1, 2, 3) == semver::semver(2, 2, 3));
    EXPECT_FALSE(semver::semver(1, 2, 3, "alpha") == semver::semver(1, 2, 3));
    EXPECT_TRUE(semver::semver(1, 2, 3, "alpha") == semver::semver(1, 2, 3, "alpha"));
    EXPECT_TRUE(semver::semver(1, 0, 0) != semver::semver(2, 0, 0));
    EXPECT_FALSE(semver::semver(1, 0, 0) != semver::semver(1, 0, 0));

    // Build metadata is ignored for equality
    EXPECT_TRUE(semver::semver(1, 2, 3, "", "a") == semver::semver(1, 2, 3, "", "b"));
    EXPECT_TRUE(semver::semver("1.2.3-beta+build") == semver::semver("1.2.3-beta+otherbuild"));
    EXPECT_TRUE(semver::semver("1.2.3+build") == semver::semver("1.2.3+otherbuild"));
}

TEST(Semver, PrecedenceSpecChain)
{
    // The precedence example from SemVer
    EXPECT_TRUE(semver::semver("1.0.0-alpha") < semver::semver("1.0.0-alpha.1"));
    EXPECT_TRUE(semver::semver("1.0.0-alpha.1") < semver::semver("1.0.0-alpha.beta"));
    EXPECT_TRUE(semver::semver("1.0.0-alpha.beta") < semver::semver("1.0.0-beta"));
    EXPECT_TRUE(semver::semver("1.0.0-beta") < semver::semver("1.0.0-beta.2"));
    EXPECT_TRUE(semver::semver("1.0.0-beta.2") < semver::semver("1.0.0-beta.11"));
    EXPECT_TRUE(semver::semver("1.0.0-beta.11") < semver::semver("1.0.0-rc.1"));
    EXPECT_TRUE(semver::semver("1.0.0-rc.1") < semver::semver("1.0.0"));
    EXPECT_TRUE(semver::semver("0.0.0") > semver::semver("0.0.0-foo"));
}

TEST(Semver, PrecedenceComparisons)
{
    // Numeric identifiers sort below alphanumeric ones.
    EXPECT_TRUE(semver::semver("1.0.0-1") < semver::semver("1.0.0-alpha"));
    EXPECT_TRUE(semver::semver("1.2.3-5") > semver::semver("1.2.3-4"));
    EXPECT_TRUE(semver::semver("1.2.3-a.10") > semver::semver("1.2.3-a.5"));
    EXPECT_TRUE(semver::semver("1.2.3-a.b") > semver::semver("1.2.3-a.5"));

    // A larger set of identifiers wins when the preceding ones are equal.
    EXPECT_TRUE(semver::semver("1.2.3-5-foo") > semver::semver("1.2.3-5"));
    EXPECT_TRUE(semver::semver("1.2.3-a.b") > semver::semver("1.2.3-a"));

    // Alphanumeric identifiers compare lexically, not by length.
    EXPECT_TRUE(semver::semver("1.2.3-r2") > semver::semver("1.2.3-r100"));
    EXPECT_TRUE(semver::semver("1.2.3-r100") > semver::semver("1.2.3-R2"));

    // ASCII case sensitivity.
    EXPECT_TRUE(semver::semver("1.2.3-5-foo") > semver::semver("1.2.3-5-Foo"));

    // The first differing identifier decides; the rest are ignored.
    EXPECT_TRUE(semver::semver("1.2.3-a.b.c.10.d.5") > semver::semver("1.2.3-a.b.c.5.d.100"));

    // Build metadata does not affect order.
    EXPECT_FALSE(semver::semver("1.0.0+build1") < semver::semver("1.0.0+build2"));
    EXPECT_FALSE(semver::semver("1.0.0+build2") < semver::semver("1.0.0+build1"));

    // Relational operators are complete.
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

    // Build metadata is ignored by the hash, like equality.
    set.clear();
    set.insert(semver::semver("1.0.0+build1"));
    set.insert(semver::semver("1.0.0+build2"));
    EXPECT_EQ(set.size(), 1);

    set.clear();
    set.insert(semver::semver("1.0.0"));
    set.insert(semver::semver("1.0.0-alpha"));
    EXPECT_EQ(set.size(), 2);
}

TEST(Semver, OrderIsTrichotomous)
{
    // Property the fuzzer checks: exactly one of a<b, a==b, b<a for valid
    // versions.
    const std::array versions{
        semver::semver("1.0.0"),         semver::semver("1.0.0-alpha"),
        semver::semver("1.0.0-alpha.1"), semver::semver("1.0.0-alpha.beta"),
        semver::semver("1.0.0-beta"),    semver::semver("1.0.0-rc.1+build.42"),
        semver::semver("1.2.3"),         semver::semver("2.0.0"),
    };

    for (const auto &a : versions) {
        for (const auto &b : versions) {
            const auto lt = a < b;
            const auto eq = a == b;
            const auto gt = b < a;
            EXPECT_EQ(static_cast<int>(lt) + static_cast<int>(eq) + static_cast<int>(gt), 1)
              << a.to_string() << " vs " << b.to_string();
        }
    }
}

TEST(Semver, HugeNumericIdentifiersCompareConsistently)
{
    // Numeric prerelease identifiers are unbounded;
    // values beyond uint64 used to overflow from_chars and compare as 0
    const semver::semver small{ "1.0.0-9" };
    const semver::semver huge_a{ "1.0.0-51611110910033187777777100" };
    const semver::semver huge_b{ "1.0.0-51611110910033187777777101" };

    EXPECT_LT(small, huge_a);
    EXPECT_LT(huge_a, huge_b);
    EXPECT_FALSE(huge_a == huge_b);

    const auto lt = huge_a < huge_b;
    const auto eq = huge_a == huge_b;
    const auto gt = huge_b < huge_a;
    EXPECT_EQ(static_cast<int>(lt) + static_cast<int>(eq) + static_cast<int>(gt), 1);
}

TEST(Semver, PartsAndParserAgreeOnRange)
{
    // The parts constructor accepts the full unsigned range; the parser must
    // accept the same values so to_string() round-trips.
    const semver::semver v{ std::numeric_limits<unsigned int>::max(), 0, 0 };
    EXPECT_EQ(semver::semver(v.to_string()), v);
}

TEST(Semver, PartsConstructorEnforcesIdentifierRules)
{
    // The parts constructor applies the same rules as the parser and uses the
    // same exception type.
    EXPECT_THROW(std::ignore = semver::semver(1, 2, 3, "rc.", ""), semver::invalid_semver);
    EXPECT_THROW(std::ignore = semver::semver(1, 2, 3, "", "build."), semver::invalid_semver);
    EXPECT_THROW(std::ignore = semver::semver(1, 2, 3, "rc..1", ""), semver::invalid_semver);
    EXPECT_THROW(std::ignore = semver::semver(1, 2, 3, "01", ""), semver::invalid_semver);
    EXPECT_THROW(std::ignore = semver::semver(1, 2, 3, "", "build+!"), semver::invalid_semver);

    // Build identifiers may have leading zeroes (SemVer 2.0.0 §10).
    EXPECT_EQ(semver::semver(1, 2, 3, "", "007").build(), "007");
}

class SemverSpecValidVersionsTest : public testing::TestWithParam<const char *>
{
};

TEST_P(SemverSpecValidVersionsTest, Accepted)
{
    const auto *const version = GetParam();
    EXPECT_NO_THROW(std::ignore = semver::semver(version)) << version;
}

INSTANTIATE_TEST_SUITE_P(Semver,
                         SemverSpecValidVersionsTest,
                         testing::Values(
                           // SemVer 2.0.0 examples
                           "1.0.0",
                           "1.0.0-alpha",
                           "1.0.0-alpha.1",
                           "1.0.0-0.3.7",
                           "1.0.0-x.7.z.92",
                           "1.0.0-x-y-z.--",
                           "1.0.0-alpha+001",
                           "1.0.0+20130313144700",
                           "1.0.0-beta+exp.sha.5114f85",
                           "1.0.0+21AF26D3----117B344092BD",
                           // reference implementation cases (dtolnay/semver)
                           "1.2.3-0",
                           "1.2.3-123",
                           "1.2.3-1.2.3",
                           "1.2.3-1a",
                           "1.2.3-alpha-1",
                           "1.2.3-alpha-.-beta",
                           "1.2.3+build.alpha.beta",
                           "1.2.3-alpha+build",
                           // large but valid values
                           "999999.999999.999999",
                           "1.0.0-a.b.c.d.e.f"));

class SemverSpecInvalidVersionsTest : public testing::TestWithParam<const char *>
{
};

TEST_P(SemverSpecInvalidVersionsTest, Rejected)
{
    const auto *const version = GetParam();
    EXPECT_THROW(std::ignore = semver::semver(version), semver::invalid_semver) << version;
}

INSTANTIATE_TEST_SUITE_P(
  Semver,
  SemverSpecInvalidVersionsTest,
  testing::Values(
    // SemVer 2.0.0 rule-derived and reference implementation cases
    "",
    "  ",
    "1",
    "1.2",
    "1.2.3-",
    "1.2.3+",
    "1.2.3-01",
    "1.2.3-0123.0123",
    "1.2.3++",
    "01.1.1",
    "1.01.1",
    "1.1.01",
    "1.2.3.DEV",
    "1.2.3 abc",
    "1.2.3 ",
    " 1.2.3",
    "v1.2.3",
    "1.2.3-alpha_beta",
    "1.0.0-alpha..",
    "1.0.0-alpha..1",
    "1.1.2+.123",
    "+invalid",
    "-invalid",
    "-invalid+invalid",
    "-invalid.01",
    "alpha",
    "alpha.beta",
    "alpha.1",
    "alpha+beta",
    "-alpha.",
    "1.2-SNAPSHOT",
    "1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788",
    "-1.0.3-gamma+b7718",
    "+justmeta",
    "9.8.7+meta+meta",
    "9.8.7-whatever+meta+meta",
    "99999999999999999999999.999999999999999999.99999999999999999----RC-SNAPSHOT.12.09.1-----------"
    "---------------------..12",
    // additional rule-derived cases
    "1.-2.3",
    "1.2.-3",
    "-0.2.3",
    "1.-0.3",
    "1.2.-0",
    "-00.0.0",
    "1.2.9999999999",
    "1.2.3abc",
    "1.2.3.4",
    "1.2.3-alpha$",
    "1.2.3-alpha space",
    "1.2.3+build$",
    // trailing-dot regression inputs
    "1.2.3-a.",
    "1.2.3-0.-0.",
    "10.66.6600-0.-0.",
    "1.2.3-rc.",
    "1.2.3+build.",
    "1.2.3-rc.1+build."));
