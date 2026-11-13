// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep
#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/utf8.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace linyaps_box {

struct utf8_model
{
    std::optional<std::string> value;
};

LINYAPS_BOX_CONFIG_SCHEMA(utf8_model,
                          utf8_model_schema,
                          "utf8_model",
                          field<&utf8_model::value>("value").ignore_null())

struct number_model
{
    std::uint64_t u64{ };
    std::uint32_t u32{ };
    std::int64_t i64{ };
    std::int32_t i32{ };
    std::optional<std::vector<std::int32_t>> list;
};

LINYAPS_BOX_CONFIG_SCHEMA(number_model,
                          number_model_schema,
                          "number_model",
                          field<&number_model::u64>("u64").required(),
                          field<&number_model::u32>("u32").required(),
                          field<&number_model::i64>("i64").required(),
                          field<&number_model::i32>("i32").required(),
                          field<&number_model::list>("list").ignore_null())

struct map_model
{
    std::unordered_map<std::string, std::string> entries;
};

LINYAPS_BOX_CONFIG_SCHEMA(map_model,
                          map_model_schema,
                          "map_model",
                          field<&map_model::entries>("entries").required())

namespace {

TEST(JsonShape, MinimalModelRejectsDecodedNul)
{
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value":"a\u0000b"})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"val\u0000ue":"x"})"),
                 json::decode_error);
}

TEST(JsonShape, MinimalModelRejectsRawNul)
{
    const std::string two_values = std::string{ R"({"value":"a"})" } + '\0' + R"({"value":"b"})";
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(two_values), json::decode_error);
}

TEST(JsonShape, MinimalModelRejectsIllFormedUtf8)
{
    const std::array<const char *, 5> documents{
        "{\"value\":\"\x80\"}",             // lone continuation byte
        "{\"value\":\"\xC0\xAF\"}",         // overlong encoding
        "{\"value\":\"\xED\xA0\x80\"}",     // UTF-8-encoded surrogate
        "{\"value\":\"\xF4\x90\x80\x80\"}", // code point above U+10FFFF
        "{\"val\x80ue\":\"x\"}",            // invalid byte in a key
    };

    for (const char *document : documents) {
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error)
          << document;
    }
}

TEST(JsonShape, MinimalModelRejectsMalformedInput)
{
    for (const char *document : { "", "{", R"({"value":})", R"({"value":"a",})", "{} {}" }) {
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error)
          << document;
    }
}

TEST(JsonShape, DecodeErrorMessageIsValidUtf8)
{
    constexpr const char *document = "{\"value\":\"\x80\"}";

    try {
        std::ignore = json::parse_as<utf8_model>(document);
        FAIL() << "expected decode_error";
    } catch (const json::decode_error &ex) {
        EXPECT_TRUE(utils::is_valid_utf8(ex.what()));
    }
}

auto with_number(std::string_view field, std::string_view raw) -> std::string
{
    std::string document = R"({"u64":0,"u32":0,"i64":0,"i32":0})";
    const auto key = "\"" + std::string{ field } + "\":";
    const auto value_begin = document.find(key) + key.size();
    const auto value_end = document.find_first_of(",}", value_begin);
    document.replace(value_begin, value_end - value_begin, raw);
    return document;
}

template <typename T>
auto parse_error_message(std::string_view document) -> std::string
{
    try {
        std::ignore = json::parse_as<T>(document);
    } catch (const json::decode_error &e) {
        return e.what();
    }

    return { };
}

TEST(JsonNumberBinding, AcceptsIntegerBoundaries)
{
    const auto lowest = json::parse_as<number_model>(
      R"({"u64":18446744073709551615,"u32":4294967295,"i64":-9223372036854775808,"i32":-2147483648})");
    EXPECT_EQ(lowest.u64, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(lowest.u32, std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(lowest.i64, std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(lowest.i32, std::numeric_limits<std::int32_t>::min());

    const auto highest = json::parse_as<number_model>(
      R"({"u64":0,"u32":0,"i64":9223372036854775807,"i32":2147483647})");
    EXPECT_EQ(highest.u64, 0U);
    EXPECT_EQ(highest.i64, std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(highest.i32, std::numeric_limits<std::int32_t>::max());
}

TEST(JsonNumberBinding, ConvertsIntegralFloatTokens)
{
    const auto model =
      json::parse_as<number_model>(R"({"u64":1.0,"u32":4294967295.0,"i64":-2.0,"i32":3.0})");
    EXPECT_EQ(model.u64, 1U);
    EXPECT_EQ(model.u32, std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(model.i64, -2);
    EXPECT_EQ(model.i32, 3);
}

TEST(JsonNumberBinding, RejectsOutOfRangeAndFractionalTokens)
{
    const std::array<std::pair<const char *, const char *>, 12> cases{
        std::pair{ "u64", "-1" },
        std::pair{ "u64", "18446744073709551616" },
        std::pair{ "u64", "18446744073709551615.0" },
        std::pair{ "u32", "-1" },
        std::pair{ "u32", "4294967296" },
        std::pair{ "u32", "4294967296.0" },
        std::pair{ "u32", "1.5" },
        std::pair{ "i64", "9223372036854775808" },
        std::pair{ "i64", "-1e19" },
        std::pair{ "i64", "-0.5" },
        std::pair{ "i32", "2147483648" },
        std::pair{ "i32", "-2147483649" },
    };

    for (const auto &[field, raw] : cases) {
        SCOPED_TRACE(std::string{ field } + "=" + raw);
        EXPECT_THROW(std::ignore = json::parse_as<number_model>(with_number(field, raw)),
                     json::decode_error);
    }
}

TEST(JsonNumberBinding, ContainerElements)
{
    const auto model = json::parse_as<number_model>(
      R"({"u64":0,"u32":0,"i64":0,"i32":0,"list":[-2147483648,0,2147483647]})");
    ASSERT_TRUE(model.list.has_value());
    ASSERT_EQ(model.list->size(), 3U);
    EXPECT_EQ((*model.list)[0], std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ((*model.list)[1], 0);
    EXPECT_EQ((*model.list)[2], std::numeric_limits<std::int32_t>::max());

    const auto empty =
      json::parse_as<number_model>(R"({"u64":0,"u32":0,"i64":0,"i32":0,"list":[]})");
    ASSERT_TRUE(empty.list.has_value());
    EXPECT_TRUE(empty.list->empty());

    const auto null_list =
      json::parse_as<number_model>(R"({"u64":0,"u32":0,"i64":0,"i32":0,"list":null})");
    EXPECT_FALSE(null_list.list.has_value());

    for (const auto *list : { R"([0,2147483648])", R"([0,"x"])", R"([1.5])" }) {
        SCOPED_TRACE(list);
        const std::string document =
          std::string{ R"({"u64":0,"u32":0,"i64":0,"i32":0,"list":)" } + list + "}";
        EXPECT_THROW(std::ignore = json::parse_as<number_model>(document), json::decode_error);
    }
}

TEST(JsonBinding, RejectsNonObjectRoot)
{
    for (const auto *document : { "[]", "1", "\"s\"", "true", "null" }) {
        SCOPED_TRACE(document);
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error);
    }
}

TEST(JsonBinding, RejectsMismatchedFieldShape)
{
    for (const auto *document : {
           R"({"value":1})",
           R"({"value":true})",
           R"({"value":[]})",
           R"({"value":{}})",
           R"({"value":[1]})",
         }) {
        SCOPED_TRACE(document);
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error);
    }
}

TEST(JsonBinding, HonoursMissingAndNullPolicy)
{
    EXPECT_FALSE(json::parse_as<utf8_model>("{}").value.has_value());
    EXPECT_FALSE(json::parse_as<utf8_model>(R"({"value":null})").value.has_value());

    EXPECT_THROW(std::ignore = json::parse_as<number_model>(R"({"u32":0,"i64":0,"i32":0})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore =
                   json::parse_as<number_model>(R"({"u64":null,"u32":0,"i64":0,"i32":0})"),
                 json::decode_error);
}

TEST(JsonShape, UnknownSubtreeStillValidatesInput)
{
    EXPECT_THROW(std::ignore =
                   json::parse_as<utf8_model>(R"({"unknown":{"a":"x\u0000y"},"value":"ok"})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore =
                   json::parse_as<utf8_model>("{\"unknown\":{\"a\":\"\x80\"},\"value\":\"ok\"}"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"unknown":[1,],"value":"ok"})"),
                 json::decode_error);

    const std::string raw_nul =
      std::string{ R"({"unknown":{"a":"x")" } + '\0' + R"("},"value":"ok"})";
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(raw_nul), json::decode_error);
}

TEST(JsonShape, UnknownKeysCoverEveryValueShape)
{
    const auto model = json::parse_as<utf8_model>(R"({
        "unknown_scalar": 1,
        "unknown_null": null,
        "unknown_empty_object": {},
        "unknown_empty_array": [],
        "unknown_nested": {"a": [{"b": [[], {}, null, "s", 1.5, true]}]},
        "value": "kept",
        "unknown_last": 2
    })");
    EXPECT_EQ(model.value, "kept");
}

TEST(JsonShape, RejectsDuplicateKnownKeys)
{
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value":"a","value":"b"})"),
                 json::decode_error);

    const auto model = json::parse_as<map_model>(R"({"entries":{"k":"a"}})");
    EXPECT_EQ(model.entries.at("k"), "a");
    EXPECT_THROW(std::ignore = json::parse_as<map_model>(R"({"entries":{"k":"a","k":"b"}})"),
                 json::decode_error);
}

TEST(JsonShape, AcceptsDuplicateUnknownKeys)
{
    const auto model = json::parse_as<utf8_model>(R"({"unknown":1,"unknown":2,"value":"x"})");
    EXPECT_EQ(model.value, "x");
}

TEST(JsonShape, RejectsExcessiveNesting)
{
    const auto nested = [](std::size_t depth) {
        std::string document = R"({"unknown":)";
        document.append(depth, '[');
        document += '0';
        document.append(depth, ']');
        document += '}';
        return document;
    };

    EXPECT_NO_THROW(std::ignore = json::parse_as<utf8_model>(nested(16)));
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(nested(256)), json::decode_error);
}

TEST(JsonShape, ErrorContextPointsAtTheOffendingNode)
{
    EXPECT_NE(parse_error_message<utf8_model>(R"({"value":1})").find("/value"), std::string::npos);
    EXPECT_NE(
      parse_error_message<number_model>(R"({"u64":0,"u32":0,"i64":0,"i32":0,"list":[0,"x"]})")
        .find("/list/1"),
      std::string::npos);
    EXPECT_NE(parse_error_message<map_model>(R"({"entries":{"k":"a","k":"b"}})").find("/entries/k"),
              std::string::npos);
    EXPECT_NE(parse_error_message<number_model>(R"({"u32":0,"i64":0,"i32":0})").find("/u64"),
              std::string::npos);
}

} // namespace
} // namespace linyaps_box
