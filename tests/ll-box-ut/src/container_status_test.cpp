// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep
#include "linyaps_box/container_status.h"
#include "linyaps_box/json/parser.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace {

auto make_status(bool with_start_time) -> linyaps_box::container_status
{
    linyaps_box::container_status s;
    s.id = "test-container";
    s.oci_version = "1.0.2";
    s.owner = "heyuming";
    s.bundle = "/tmp/test-bundle";
    s.pid = 4242;
    s.created = std::chrono::system_clock::time_point{ std::chrono::nanoseconds{
      1'700'000'000'123'456'000 } };
    s.annotations = { { "key", "value" } };
    if (with_start_time) {
        s.process_start_time = std::uint64_t{ 123456789 };
    }

    return s;
}

} // namespace

TEST(ContainerStatusJson, RoundTripWithStartTime)
{
    auto s = make_status(true);
    const auto ret = nlohmann::json(s).dump();
    auto parsed = linyaps_box::json::parse_as<linyaps_box::container_status>(ret);

    EXPECT_EQ(parsed.id, s.id);
    EXPECT_EQ(parsed.oci_version, s.oci_version);
    EXPECT_EQ(parsed.owner, s.owner);
    EXPECT_EQ(parsed.bundle, s.bundle);
    EXPECT_EQ(parsed.pid, s.pid);
    EXPECT_EQ(parsed.created, s.created);
    EXPECT_EQ(parsed.annotations, s.annotations);
    ASSERT_TRUE(parsed.process_start_time.has_value());
    EXPECT_EQ(*parsed.process_start_time, std::uint64_t{ 123456789 });
}

TEST(ContainerStatusJson, OmitKeyWhenNullopt)
{
    auto s = make_status(false);
    const auto j = nlohmann::json(s);

    EXPECT_FALSE(j.contains("process-start-time"));
}

TEST(ContainerStatusJson, IncludeKeyWhenSet)
{
    auto s = make_status(true);
    const auto j = nlohmann::json(s);

    EXPECT_TRUE(j.contains("process-start-time"));
    EXPECT_EQ(j.at("process-start-time").get<std::uint64_t>(), std::uint64_t{ 123456789 });
}

TEST(ContainerStatusJson, MissingKeyYieldsNullopt)
{
    const auto *text = R"({"id":"c1","pid":100,"bundle":"/b",)"
                       R"("created":"2023-11-14T22:13:20.123456Z","owner":"o",)"
                       R"("annotations":{},"ociVersion":"1.0.2"})";

    const auto parsed = linyaps_box::json::parse_as<linyaps_box::container_status>(text);

    EXPECT_FALSE(parsed.process_start_time.has_value());
}

// An explicit null for process-start-time fails (it is not "missing").
TEST(ContainerStatusJson, ExplicitNullThrows)
{
    const auto *text = R"({"id":"c1","pid":100,"process-start-time":null,"bundle":"/b",)"
                       R"("created":"2023-11-14T22:13:20.123456Z","owner":"o",)"
                       R"("annotations":{},"ociVersion":"1.0.2"})";

    EXPECT_THROW(std::ignore = linyaps_box::json::parse_as<linyaps_box::container_status>(text),
                 linyaps_box::json::decode_error);
}
