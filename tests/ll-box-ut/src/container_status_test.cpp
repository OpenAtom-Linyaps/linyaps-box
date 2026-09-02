// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/container_status.h"

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
    nlohmann::json j;
    linyaps_box::to_json(j, s);

    linyaps_box::container_status parsed;
    linyaps_box::from_json(j, parsed);

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
    nlohmann::json j;
    linyaps_box::to_json(j, s);

    EXPECT_FALSE(j.contains("process-start-time"));
}

TEST(ContainerStatusJson, IncludeKeyWhenSet)
{
    auto s = make_status(true);
    nlohmann::json j;
    linyaps_box::to_json(j, s);

    EXPECT_TRUE(j.contains("process-start-time"));
    EXPECT_EQ(j.at("process-start-time").get<std::uint64_t>(), std::uint64_t{ 123456789 });
}

TEST(ContainerStatusJson, MissingKeyYieldsNullopt)
{
    auto j = nlohmann::json::object({ { "id", "c1" },
                                      { "pid", 100 },
                                      { "bundle", "/b" },
                                      { "created", "2023-11-14T22:13:20.123456Z" },
                                      { "owner", "o" },
                                      { "annotations", nlohmann::json::object() },
                                      { "ociVersion", "1.0.2" } });

    linyaps_box::container_status parsed;
    linyaps_box::from_json(j, parsed);

    EXPECT_FALSE(parsed.process_start_time.has_value());
}

TEST(ContainerStatusJson, ExplicitNullThrows)
{
    auto j = nlohmann::json::object({ { "id", "c1" },
                                      { "pid", 100 },
                                      { "process-start-time", nullptr },
                                      { "bundle", "/b" },
                                      { "created", "2023-11-14T22:13:20.123456Z" },
                                      { "owner", "o" },
                                      { "annotations", nlohmann::json::object() },
                                      { "ociVersion", "1.0.2" } });

    linyaps_box::container_status parsed;
    EXPECT_THROW(linyaps_box::from_json(j, parsed), nlohmann::json::type_error);
}
