// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep
#include "linyaps_box/utils/utf8.h"

#include <string>
#include <string_view>

namespace {

TEST(ToValidUtf8Lossy, CopiesValidTextUnchanged)
{
    using linyaps_box::utils::to_valid_utf8_lossy;

    std::string buffer;
    const auto check = [&buffer](std::string_view text) {
        to_valid_utf8_lossy(text, buffer);
        EXPECT_EQ(buffer, text);
    };

    check("");
    check("plain");
    check("plain ascii, over the eight-byte boundary");
    check("\xC3\xA9");
    check("\xE2\x82\xAC");
    check("\xF0\x9F\x98\x80");
    check(std::string_view{ "a\0b", 3 });

    // Code points at the edge of each encoded range.
    check("\xC2\x80");         // U+0080
    check("\xDF\xBF");         // U+07FF
    check("\xE0\xA0\x80");     // U+0800
    check("\xED\x9F\xBF");     // U+D7FF
    check("\xEE\x80\x80");     // U+E000
    check("\xEF\xBF\xBF");     // U+FFFF
    check("\xF0\x90\x80\x80"); // U+10000
    check("\xF4\x8F\xBF\xBF"); // U+10FFFF
}

TEST(ToValidUtf8Lossy, ReplacesEachMaximalSubpartOnce)
{
    using linyaps_box::utils::to_valid_utf8_lossy;

    const std::string r = "\xEF\xBF\xBD";
    std::string buffer;
    const auto check = [&buffer](std::string_view text, std::string_view expected) {
        to_valid_utf8_lossy(text, buffer);
        EXPECT_EQ(buffer, expected);
    };

    check("\xE2", r);
    check("\xE2\x82", r);
    check("\xF0", r);
    check("\xF0\x90", r);
    check("\xF0\x90\x80", r);
    check("\xF0\x9F\x98", r);
    check("\x80", r);

    // A break inside a 4-byte sequence stops at the maximal subpart.
    check("\xF0\x41\x80\x80", r + "A" + r + r);
    check("\xF0\x90\x41\x80", r + "A" + r);
    check("\xE1\x41\x80", r + "A" + r);

    check("\xC2\x41", r + "A");
    check("\xE2\x28\xA1", r + "(" + r);
    check(std::string{ "\xE2\x82" } + "b", r + "b");

    check("\xC0\xAF", r + r);
    check("\xC1\xBF", r + r);
    check("\xE0\x9F\xBF", r + r + r);
    check("\xF0\x8F\xBF\xBF", r + r + r + r);
    check("\xE0\x80\x80", r + r + r);
    check("\xED\xA0\x80", r + r + r);
    check("\xF4\x90\x80\x80", r + r + r + r);
    check("\xF5", r);
    check("\xFF", r);

    for (const std::size_t ascii_bytes : { 0U, 1U, 7U, 8U, 9U, 15U, 16U, 17U }) {
        SCOPED_TRACE(ascii_bytes);
        const std::string ascii(ascii_bytes, 'x');
        check(ascii + "\x80", ascii + r);
        check(std::string{ "\x80" } + ascii, r + ascii);
    }

    for (const auto *bad : { "\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82" }) {
        to_valid_utf8_lossy(bad, buffer);
        const auto first = buffer;

        to_valid_utf8_lossy(first, buffer);
        EXPECT_EQ(buffer, first);
    }
}

TEST(ToValidUtf8Lossy, EscapesNulWhenAsked)
{
    using linyaps_box::utils::nul_rule;
    using linyaps_box::utils::to_valid_utf8_lossy;

    std::string buffer;
    const auto escape = [&buffer](std::string_view text) {
        to_valid_utf8_lossy(text, buffer, nul_rule::forbidden);
        return buffer;
    };

    EXPECT_EQ(escape(std::string_view{ "a\0b", 3 }), "a\\u0000b");
    EXPECT_EQ(escape(std::string_view{ "\0", 1 }), "\\u0000");
    EXPECT_EQ(escape(std::string_view{ "\0x", 2 }), "\\u0000x");
    EXPECT_EQ(escape(std::string_view{ "x\0", 2 }), "x\\u0000");
    EXPECT_EQ(escape(std::string{ "a" } + '\0' + "\x80" + "b"),
              std::string{ "a\\u0000" } + "\xEF\xBF\xBD" + "b");

    const std::string with_nul{ "a\0b", 3 };
    to_valid_utf8_lossy(with_nul, buffer);
    EXPECT_EQ(buffer, with_nul);
}

TEST(ToValidUtf8Lossy, ReusesOutputBuffer)
{
    using linyaps_box::utils::to_valid_utf8_lossy;

    std::string buffer(512, 'z');

    const std::string long_text(300, 'a');
    to_valid_utf8_lossy(long_text, buffer);
    EXPECT_EQ(buffer, long_text);

    to_valid_utf8_lossy("short", buffer);
    EXPECT_EQ(buffer, "short");

    to_valid_utf8_lossy("", buffer);
    EXPECT_TRUE(buffer.empty());
}

TEST(ToValidUtf8Lossy, ReturningOverloadForwardsNulRule)
{
    using linyaps_box::utils::nul_rule;
    using linyaps_box::utils::to_valid_utf8_lossy;

    const std::string with_nul{ "a\0b", 3 };

    EXPECT_EQ(to_valid_utf8_lossy(with_nul), with_nul);
    EXPECT_EQ(to_valid_utf8_lossy(with_nul, nul_rule::forbidden), "a\\u0000b");
}

TEST(ToValidUtf8Lossy, EscapesNulAtWordBoundaries)
{
    using linyaps_box::utils::nul_rule;
    using linyaps_box::utils::to_valid_utf8_lossy;

    for (const std::size_t prefix : { 0U, 1U, 7U, 8U, 9U, 15U, 16U, 17U }) {
        SCOPED_TRACE(prefix);
        const std::string text = std::string(prefix, 'x') + '\0' + "y";
        EXPECT_EQ(to_valid_utf8_lossy(text, nul_rule::forbidden),
                  std::string(prefix, 'x') + "\\u0000y");
    }

    EXPECT_EQ(to_valid_utf8_lossy(std::string_view{ "\0\0", 2 }, nul_rule::forbidden),
              "\\u0000\\u0000");
}

TEST(Utf8Validity, ForbidsNulWhenAsked)
{
    using linyaps_box::utils::is_valid_utf8;
    using linyaps_box::utils::nul_rule;

    const std::string with_nul{ "a\0b", 3 };
    EXPECT_TRUE(is_valid_utf8(with_nul)); // NUL is well-formed UTF-8
    EXPECT_FALSE(is_valid_utf8(with_nul, nul_rule::forbidden));
    EXPECT_TRUE(is_valid_utf8("ab", nul_rule::forbidden));
    EXPECT_TRUE(is_valid_utf8("\xC3\xA9", nul_rule::forbidden));
}

TEST(Utf8Validity, AcceptsWellFormedAndRejectsIllFormed)
{
    using linyaps_box::utils::is_valid_utf8;

    EXPECT_TRUE(is_valid_utf8(""));
    EXPECT_TRUE(is_valid_utf8("plain ascii, over the eight-byte boundary"));
    EXPECT_TRUE(is_valid_utf8(std::string{ "a" } + '\0' + "b")); // NUL is valid UTF-8
    EXPECT_TRUE(is_valid_utf8("\xC3\xA9"));
    EXPECT_TRUE(is_valid_utf8("\xE2\x82\xAC"));
    EXPECT_TRUE(is_valid_utf8("\xF0\x9F\x98\x80"));

    EXPECT_TRUE(is_valid_utf8("\xC2\x80"));                               // U+0080
    EXPECT_TRUE(is_valid_utf8("\xDF\xBF"));                               // U+07FF
    EXPECT_TRUE(is_valid_utf8("\xE0\xA0\x80"));                           // U+0800
    EXPECT_TRUE(is_valid_utf8("\xED\x9F\xBF"));                           // U+D7FF
    EXPECT_TRUE(is_valid_utf8("\xEE\x80\x80"));                           // U+E000
    EXPECT_TRUE(is_valid_utf8("\xEF\xBF\xBF"));                           // U+FFFF
    EXPECT_TRUE(is_valid_utf8("\xF0\x90\x80\x80"));                       // U+10000
    EXPECT_TRUE(is_valid_utf8("\xF4\x8F\xBF\xBF"));                       // U+10FFFF
    EXPECT_TRUE(is_valid_utf8(std::string_view{ "ascii\0\xC3\xA9", 8 })); // NUL inside a run

    for (const auto *bad : {
           "\x80",
           "\xC0\xAF",
           "\xC1\xBF",         // overlong 2-byte
           "\xE0\x9F\xBF",     // just below U+0800
           "\xF0\x8F\xBF\xBF", // just below U+10000
           "\xED\xA0\x80",
           "\xF4\x90\x80\x80",
           "\xE2\x82",
           "\xE2",
           "\xFF",
         }) {
        EXPECT_FALSE(is_valid_utf8(bad));
    }
}

} // namespace
