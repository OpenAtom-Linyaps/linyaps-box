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

struct inner_model
{
    std::optional<std::string> name;
};

LINYAPS_BOX_CONFIG_SCHEMA(inner_model,
                          inner_model_schema,
                          "inner_model",
                          field<&inner_model::name>("name").ignore_null())

struct shapes_model
{
    std::optional<std::vector<inner_model>> objects;
    std::optional<std::unordered_map<std::string, inner_model>> by_name;
    std::optional<std::vector<std::uint8_t>> bytes;
    std::optional<inner_model> inner;
    std::optional<bool> flag;
};

LINYAPS_BOX_CONFIG_SCHEMA(shapes_model,
                          shapes_model_schema,
                          "shapes_model",
                          field<&shapes_model::objects>("objects").ignore_null(),
                          field<&shapes_model::by_name>("byName").ignore_null(),
                          field<&shapes_model::bytes>("bytes").ignore_null(),
                          field<&shapes_model::inner>("inner").ignore_null(),
                          field<&shapes_model::flag>("flag").ignore_null())

namespace {

enum class shape_kind : std::uint8_t { first, second };

LINYAPS_REGISTER_ENUM_TABLE(shape_kind,
                            2,
                            { shape_kind::first, "FIRST" },
                            { shape_kind::second, "SECOND" })

struct enum_map_model
{
    std::unordered_map<std::string, shape_kind> entries;
};

LINYAPS_BOX_CONFIG_SCHEMA(enum_map_model,
                          enum_map_model_schema,
                          "enum_map_model",
                          field<&enum_map_model::entries>("entries").required())

struct enum_member_model
{
    shape_kind kind{ shape_kind::first };
};

LINYAPS_BOX_CONFIG_SCHEMA(enum_member_model,
                          enum_member_model_schema,
                          "enum_member_model",
                          field<&enum_member_model::kind>("kind").required())

struct double_model
{
    double value{ 0 };
};

LINYAPS_BOX_CONFIG_SCHEMA(double_model,
                          double_model_schema,
                          "double_model",
                          field<&double_model::value>("value").required())

struct post_parse_model
{
    std::optional<int> value;
};

void reject_negative(post_parse_model &v) // NOLINT
{
    if (v.value.value_or(0) < 0) {
        throw json::decode_error{ "value must not be negative" };
    }
}

LINYAPS_BOX_CONFIG_SCHEMA_WITH_POST(post_parse_model,
                                    post_parse_model_schema,
                                    "post_parse_model",
                                    reject_negative,
                                    field<&post_parse_model::value>("value").ignore_null())

struct strings_model
{
    std::string joined;
    int count{ 0 };
};

void collect_string(std::string_view s, strings_model &m)
{
    if (s == "reject") {
        throw json::decode_error{ "string rejected by consumer" };
    }

    if (m.count != 0) {
        m.joined.push_back(',');
    }

    m.joined.append(s);
    ++m.count;
}

LINYAPS_BOX_CONFIG_SCHEMA(strings_model,
                          strings_model_schema,
                          "strings_model",
                          json::consume_strings<&collect_string>("items").ignore_null())

struct strings_list_model
{
    std::vector<strings_model> list;
};

LINYAPS_BOX_CONFIG_SCHEMA(strings_list_model,
                          strings_list_model_schema,
                          "strings_list_model",
                          field<&strings_list_model::list>("list").required())

struct nested_strings_model
{
    std::vector<nested_strings_model> children;
    std::optional<strings_model> leaf;
};

LINYAPS_BOX_CONFIG_SCHEMA(nested_strings_model,
                          nested_strings_schema,
                          "nested_strings_model",
                          field<&nested_strings_model::children>("children"),
                          field<&nested_strings_model::leaf>("leaf"))

struct facade_model
{
    std::int64_t n{ 0 };
};

auto read_integer(const json::value_view &v) -> std::int64_t
{
    return v.as_integer<std::int64_t>();
}

LINYAPS_BOX_CONFIG_SCHEMA(facade_model,
                          facade_model_schema,
                          "facade_model",
                          field<&facade_model::n, &read_integer>("n").required())

} // namespace

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
    EXPECT_NE(parse_error_message<shapes_model>(R"({"byName":{"a/b~":{"name":1}}})")
                .find("at '/byName/a~1b~0/name':"),
              std::string::npos);
    EXPECT_NE(parse_error_message<number_model>(R"({"u32":0,"i64":0,"i32":0})").find("/u64"),
              std::string::npos);
    EXPECT_NE(parse_error_message<number_model>(R"({"u64":0,"i32":0})").find("at '/u32':"),
              std::string::npos);
}

TEST(JsonShapes, ArrayFieldRejectsWrongShapes)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"objects":{}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"objects":"x"})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"objects":[1]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"objects":[[]]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"objects":[{"name":1}]})"),
                 json::decode_error);
}

TEST(JsonShapes, MapFieldRejectsWrongShapes)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"byName":[]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"byName":{"k":1}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"byName":{"k":[]}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"byName":{"k":{"name":1}}})"),
                 json::decode_error);
}

TEST(JsonShapes, AcceptsContainerShapes)
{
    const auto model = json::parse_as<shapes_model>(
      R"({"objects":[{"name":"a"},{}],"byName":{"k":{"name":"b"}},"bytes":[1,255],"flag":true})");
    ASSERT_TRUE(model.objects.has_value());
    EXPECT_EQ(model.objects->size(), 2U);
    ASSERT_TRUE(model.by_name.has_value());
    EXPECT_EQ(model.by_name->at("k").name, std::optional<std::string>{ "b" });
    ASSERT_TRUE(model.bytes.has_value());
    EXPECT_EQ(model.bytes->at(1), 255U);
    EXPECT_TRUE(model.flag.value_or(false));
}

TEST(JsonNumberBinding, RejectsValuesOutOfRangeForTheMemberType)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[256]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[-1]})"),
                 json::decode_error);
}

TEST(JsonShape, BooleanFieldRejectsNonBooleans)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"flag":1})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"flag":"true"})"),
                 json::decode_error);
}

TEST(JsonShape, RejectsExcessivelyNestedObjects)
{
    std::string document = R"({"unknown":)";
    for (int i = 0; i < 256; ++i) {
        document += R"({"unknown":)";
    }

    document += '1';
    for (int i = 0; i < 256; ++i) {
        document += '}';
    }

    document += '}';

    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error);
}

TEST(JsonShapes, ScalarElementsRejectContainerValues)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[{}]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[[]]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":["x"]})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"byName":{"k":[]}})"),
                 json::decode_error);
}

TEST(JsonShapes, ObjectFieldRejectsNonObjectShapes)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"inner":[]})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"inner":1})"), json::decode_error);

    const auto model = json::parse_as<shapes_model>(R"({"inner":{"name":"n"}})");
    ASSERT_TRUE(model.inner.has_value());
    EXPECT_EQ(model.inner->name, std::optional<std::string>{ "n" });
}

TEST(JsonNumberBinding, RejectsHugeUnsignedValues)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[18446744073709551615]})"),
                 json::decode_error);
}

TEST(JsonShapes, ScalarFieldRejectsContainerShapes)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"flag":{}})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"flag":[]})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":{}})"), json::decode_error);
}

TEST(JsonBinding, MapValueConversionFailureIsRejected)
{
    EXPECT_THROW(std::ignore = json::parse_as<enum_map_model>(R"({"entries":{"k":"NOPE"}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<map_model>(R"({"entries":{"k":{}}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<map_model>(R"({"entries":{"k":[]}})"),
                 json::decode_error);

    const auto model = json::parse_as<enum_map_model>(R"({"entries":{"k":"SECOND"}})");
    EXPECT_EQ(model.entries.count("k"), 1U);
    EXPECT_TRUE(model.entries.at("k") == shape_kind::second);
}

TEST(JsonBinding, PostParseRejectionIsReported)
{
    EXPECT_THROW(std::ignore = json::parse_as<post_parse_model>(R"({"value":-1})"),
                 json::decode_error);

    const auto model = json::parse_as<post_parse_model>(R"({"value":1})");
    EXPECT_EQ(model.value.value_or(0), 1);
}

TEST(JsonNumberBinding, RejectsValuesAboveInt64ForUnsignedMembers)
{
    EXPECT_THROW(std::ignore = json::parse_as<shapes_model>(R"({"bytes":[9223372036854775808]})"),
                 json::decode_error);
}

TEST(JsonGrammar, NumberTokensFollowTheGrammar)
{
    for (const char *raw : {
           "01",   "00",  "+1",       ".5",    "5.",    "1.",   "1e",    "1e+", "1e-",      "--1",
           "0x10", "NaN", "Infinity", "1_000", "1.2.3", "01.5", "1e1e1", "0.",  "\xD9\xA3",
         }) {
        EXPECT_THROW(std::ignore = json::parse_as<number_model>(with_number("u64", raw)),
                     json::decode_error)
          << raw;
    }

    EXPECT_EQ(json::parse_as<number_model>(with_number("u64", "-0")).u64, 0U);
    EXPECT_EQ(json::parse_as<number_model>(with_number("i32", "-0")).i32, 0);
    EXPECT_EQ(json::parse_as<number_model>(with_number("u64", "1E+2")).u64, 100U);
    EXPECT_EQ(json::parse_as<number_model>(with_number("u64", "1e2")).u64, 100U);
    EXPECT_EQ(json::parse_as<number_model>(with_number("i32", "1.0e1")).i32, 10);
}

TEST(JsonGrammar, StringEscapesAreValidated)
{
    for (const char *document : {
           R"({"value":"\x"})",
           R"({"value":"\'"})",
           R"({"value":"\u12"})",
           R"({"value":"\uZZZZ"})",
           R"({"value":"\U0041"})",
         }) {
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error)
          << document;
    }

    for (const char control : { '\x01', '\x1f' }) {
        std::string document = R"({"value":"a)";
        document += control;
        document += R"("})";
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error);
    }

    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>("{\"value\":\"a\nb\"}"),
                 json::decode_error);

    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value":"\uD800"})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value":"\uDC00"})"),
                 json::decode_error);

    EXPECT_EQ(json::parse_as<utf8_model>(R"({"value":"a\tb"})").value,
              std::optional<std::string>{ "a\tb" });
    EXPECT_EQ(json::parse_as<utf8_model>(R"({"value":"\u0041"})").value,
              std::optional<std::string>{ "A" });
    EXPECT_EQ(json::parse_as<utf8_model>(R"({"value":"\uD83D\uDE00"})").value,
              std::optional<std::string>{ "\xF0\x9F\x98\x80" });
}

TEST(JsonGrammar, StructureFollowsTheGrammar)
{
    for (const char *document : {
           R"({value:1})",
           R"({"value" 1})",
           R"({"value"::1})",
           R"({"value":1,})",
           R"([1,])",
           R"({'value':"x"})",
           R"({"value":"x"}trailing)",
           R"(// comment)",
           R"({"value":/*c*/"x"})",
           R"({"value":True})",
           R"({"value":NaN})",
           R"({"value":})",
           R"({"value":1"})",
         }) {
        EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(document), json::decode_error)
          << document;
    }
}

TEST(JsonShape, RespectsTheViewLength)
{
    const std::string buffer = std::string{ R"({"value":"x"})" } + "garbage";
    const std::string_view document{ buffer.data(), buffer.size() - 7 };

    const auto model = json::parse_as<utf8_model>(document);
    EXPECT_EQ(model.value, std::optional<std::string>{ "x" });
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(buffer), json::decode_error);
}

TEST(JsonShape, ParsingIsStateless)
{
    constexpr std::string_view document = R"({"value":"x"})";

    const auto first = json::parse_as<utf8_model>(document);
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value":1})"), json::decode_error);
    const auto second = json::parse_as<utf8_model>(document);

    EXPECT_EQ(first.value, std::optional<std::string>{ "x" });
    EXPECT_EQ(second.value, first.value);
}

TEST(JsonStrings, ConsumeStringsInOrder)
{
    const auto model = json::parse_as<strings_model>(R"({"items": ["a", "b", "c"]})");
    EXPECT_EQ(model.joined, "a,b,c");
    EXPECT_EQ(model.count, 3);
}

TEST(JsonStrings, EmptyMissingAndNullItems)
{
    for (const char *document : { R"({"items": []})", R"({})", R"({"items": null})" }) {
        const auto model = json::parse_as<strings_model>(document);
        EXPECT_EQ(model.joined, "") << document;
        EXPECT_EQ(model.count, 0) << document;
    }
}

TEST(JsonStrings, RejectsWrongShapesAndElements)
{
    for (const char *document : {
           R"({"items": "x"})",
           R"({"items": 1})",
           R"({"items": [1]})",
           R"({"items": [null]})",
           R"({"items": [{}]})",
           R"({"items": [[]]})",
           R"({"items": [true]})",
         }) {
        EXPECT_THROW(std::ignore = json::parse_as<strings_model>(document), json::decode_error)
          << document;
    }
}

TEST(JsonStrings, EachArrayWritesToItsOwnObject)
{
    const auto model = json::parse_as<strings_list_model>(
      R"({"list": [{"items": ["a"]}, {"items": ["b", "c"]}, {"items": []}]})");
    ASSERT_EQ(model.list.size(), 3U);
    EXPECT_EQ(model.list[0].joined, "a");
    EXPECT_EQ(model.list[0].count, 1);
    EXPECT_EQ(model.list[1].joined, "b,c");
    EXPECT_EQ(model.list[1].count, 2);
    EXPECT_EQ(model.list[2].count, 0);
}

TEST(JsonStrings, CanParseAgainAfterFailure)
{
    EXPECT_THROW(std::ignore = json::parse_as<strings_list_model>(
                   R"({"list": [{"items": ["a"]}, {"items": ["b", 1]}]})"),
                 json::decode_error);
    const auto model = json::parse_as<strings_model>(R"({"items": ["c"]})");
    EXPECT_EQ(model.joined, "c");
    EXPECT_EQ(model.count, 1);
}

TEST(JsonStrings, LocatesConsumerFailureInDeeplyNestedObject)
{
    std::string input = R"({"leaf":{"items":["ok","reject"]}})";
    std::string path;
    for (int i = 0; i < 7; ++i) {
        input = R"({"children":[)" + input + "]}";
        path += "/children/0";
    }

    const auto message = parse_error_message<nested_strings_model>(input);
    EXPECT_NE(message.find("at '" + path + "/leaf/items/1':"), std::string::npos);
    EXPECT_NE(message.find("string rejected by consumer"), std::string::npos);
}

TEST(JsonShape, ValueViewFacadeReadsCheckedIntegers)
{
    EXPECT_EQ(json::parse_as<facade_model>(R"({"n": 42})").n, 42);
    EXPECT_EQ(json::parse_as<facade_model>(R"({"n": 1.0})").n, 1);
    EXPECT_THROW(std::ignore = json::parse_as<facade_model>(R"({"n": 1.5})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<facade_model>(R"({"n": "x"})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<facade_model>(R"({"n": true})"), json::decode_error);
}

TEST(JsonShapes, ContainerOnAConvertedFieldIsRejected)
{
    EXPECT_THROW(std::ignore = json::parse_as<facade_model>(R"({"n": {}})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<facade_model>(R"({"n": []})"), json::decode_error);
}

TEST(JsonShapes, ContainerOnAConsumeStringsFieldIsRejected)
{
    EXPECT_THROW(std::ignore = json::parse_as<strings_model>(R"({"items": {}})"),
                 json::decode_error);
}

TEST(JsonShapes, ContainerOnAnEnumMemberIsRejected)
{
    EXPECT_THROW(std::ignore = json::parse_as<enum_member_model>(R"({"kind": {}})"),
                 json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<enum_member_model>(R"({"kind": []})"),
                 json::decode_error);
}

TEST(JsonShape, ScalarTypeNameCoversSignedAndFloat)
{
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value": -1})"), json::decode_error);
    EXPECT_THROW(std::ignore = json::parse_as<utf8_model>(R"({"value": 1.5})"), json::decode_error);
}

TEST(JsonNumberBinding, BindsFloatingMembers)
{
    EXPECT_DOUBLE_EQ(json::parse_as<double_model>(R"({"value": 1.5})").value, 1.5);
    EXPECT_DOUBLE_EQ(json::parse_as<double_model>(R"({"value": 2})").value, 2.0);
    // A signed integer and a non-number both reach the floating arm too.
    EXPECT_DOUBLE_EQ(json::parse_as<double_model>(R"({"value": -1})").value, -1.0);
    EXPECT_THROW(std::ignore = json::parse_as<double_model>(R"({"value": "x"})"),
                 json::decode_error);
}

} // namespace
} // namespace linyaps_box
