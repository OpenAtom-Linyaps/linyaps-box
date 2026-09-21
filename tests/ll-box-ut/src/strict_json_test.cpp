// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linyaps_box/config/user.h"
#include "linyaps_box/utils/strict_json.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace linyaps_box {
namespace {

using json = linyaps_box::utils::strict_json;

using utils::require_object;
using utils::strict_parse;

TEST(JsonNumber, FloatOutOfRangeRejected)
{
    EXPECT_THROW(std::ignore = json(std::numeric_limits<double>::infinity()).get<std::int64_t>(),
                 json::exception);
    EXPECT_THROW(std::ignore = json::parse("-1e300").get<std::uint64_t>(), json::exception);

    EXPECT_THROW(std::ignore = json::parse("18446744073709551616.0").get<std::uint64_t>(),
                 json::exception); // 2^64
    EXPECT_THROW(std::ignore = json::parse("9223372036854775808.0").get<std::int64_t>(),
                 json::exception); // 2^63
    EXPECT_EQ(9223372036854774784LL, json::parse("9223372036854774784.0").get<std::int64_t>());
}

TEST(JsonNumber, FloatFractionRejected)
{
    EXPECT_THROW(std::ignore = json::parse("3.14").get<std::int64_t>(), json::exception);
}

TEST(JsonNumber, FloatIntegralValueAccepted)
{
    EXPECT_EQ(5, json::parse("5.0").get<std::int64_t>());
}

TEST(JsonNumber, UnsignedOverflowRejected)
{
    EXPECT_THROW(std::ignore = json::parse("256").get<std::uint8_t>(), json::exception);
    EXPECT_EQ(255, json::parse("255").get<std::uint8_t>());
}

TEST(JsonNumber, SignedRangeRejected)
{
    EXPECT_EQ(-128, json::parse("-128").get<std::int8_t>());
    EXPECT_THROW(std::ignore = json::parse("-129").get<std::int8_t>(), json::exception);
    EXPECT_THROW(std::ignore = json::parse("-1").get<std::uint32_t>(), json::exception);
}

TEST(JsonNumber, BooleanNotAcceptedAsInteger)
{
    EXPECT_THROW(std::ignore = json::parse("true").get<std::int32_t>(), json::exception);
}

TEST(JsonNumber, ContainerElementsChecked)
{
    EXPECT_THROW(std::ignore = json::parse("[10,4294967296]").get<std::vector<std::uint32_t>>(),
                 json::exception);
    EXPECT_EQ((std::vector<std::uint32_t>{ 10, 20 }),
              json::parse("[10,20]").get<std::vector<std::uint32_t>>());
}

TEST(JsonNumber, JsonValueHelperChecked)
{
    EXPECT_THROW(std::ignore = json::parse(R"({"k":4294967296})").value("k", std::uint32_t{ 0 }),
                 json::exception);
    EXPECT_EQ(7U, json::parse(R"({"k":7})").value("k", std::uint32_t{ 0 }));
}

TEST(JsonShape, RequireObjectRejected)
{
    for (const char *text : { "42", "\"x\"", "[]", "null", "true" }) {
        EXPECT_THROW(require_object(json::parse(text)), json::type_error) << text;
    }

    EXPECT_NO_THROW(require_object(json::parse("{}")));
}

// End-to-end guard for the type-encoded invariant: config from_json must run on
// utils::strict_json, so container-element and scalar extraction are checked.
// If a from_json ever regresses to a plain nlohmann::json parameter, this fails.
TEST(JsonShape, ConfigExtractionIsChecked)
{
    // additionalGids is std::vector<gid_t>; 2^32 does not fit in gid_t.
    EXPECT_THROW(
      std::ignore =
        strict_parse(R"({"uid":0,"gid":0,"additionalGids":[4294967296]})").get<config::user>(),
      json::exception);

    EXPECT_NO_THROW(
      std::ignore =
        strict_parse(R"({"uid":0,"gid":0,"additionalGids":[1,2]})").get<config::user>());
}

TEST(JsonShape, ParseRejectsDuplicateKeys)
{
    EXPECT_THROW(std::ignore = strict_parse(R"({"a":1,"a":2})"), json::exception);
    EXPECT_THROW(std::ignore = strict_parse(R"({"o":{"a":1,"a":2}})"), json::exception);

    EXPECT_NO_THROW(std::ignore = strict_parse(R"({"a":{"x":1},"b":{"x":2}})"));
}

TEST(JsonShape, DuplicateKeyScopesDoNotLeakAcrossArrays)
{
    EXPECT_THROW(std::ignore = strict_parse(R"([{"a":1,"a":2}])"), json::exception);
    EXPECT_NO_THROW(std::ignore = strict_parse(R"([{"a":1},{"a":2}])"));
    EXPECT_NO_THROW(std::ignore = strict_parse(R"({"o":[{"a":1},{"a":2}]})"));
}

TEST(JsonShape, ParseRejectsMalformedInput)
{
    EXPECT_THROW(std::ignore = strict_parse(""), json::exception);
    EXPECT_THROW(std::ignore = strict_parse("{"), json::exception);
    EXPECT_THROW(std::ignore = strict_parse(R"({"a":})"), json::exception);
    EXPECT_THROW(std::ignore = strict_parse(R"({"a":1,})"), json::exception);
    EXPECT_THROW(std::ignore = strict_parse("{} {}"), json::exception);
}

TEST(JsonShape, ParseRejectsEmbeddedNul)
{
    EXPECT_THROW(std::ignore = strict_parse(R"({"a":"x\u0000y"})"), json::exception);
    EXPECT_THROW(std::ignore = strict_parse(R"({"a\u0000b":1})"), json::exception);
}

TEST(JsonShape, ParseMatchesNlohmann)
{
    for (const char *text : {
           "null",
           "true",
           "false",
           "0",
           "-0",
           "1e10",
           "-2.5e-3",
           "\"\"",
           R"("\u00e9\u4e2d")",
           "[]",
           "{}",
           "[[]]",
           "[{},[],1,\"x\",null]",
           R"({"a":[],"b":{},"c":[{"d":1}]})",
           R"({"a":{"b":{"c":{"d":[1,2,{"e":"f"}]}}}})",
           R"([{"a":1},{"a":2},{"b":[{"c":3}]}])",
         }) {
        EXPECT_EQ(strict_parse(text), linyaps_box::utils::strict_json::parse(text)) << text;
    }

    // Deep nesting exercises the ref_stack push/pop balance.
    std::string deep;
    for (int i = 0; i < 500; ++i) {
        deep += '[';
    }

    deep += '1';

    for (int i = 0; i < 500; ++i) {
        deep += ']';
    }

    EXPECT_EQ(strict_parse(deep), linyaps_box::utils::strict_json::parse(deep));
}

} // namespace
} // namespace linyaps_box
