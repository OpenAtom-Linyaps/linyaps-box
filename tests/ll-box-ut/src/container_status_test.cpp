// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/container_status.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/utils/time.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include <unistd.h>

namespace {

using linyaps_box::container_status;
using linyaps_box::runtime_status;

class TempDir
{
public:
    TempDir(const TempDir &) = delete;
    TempDir(TempDir &&) noexcept = default;
    TempDir &operator=(const TempDir &) = delete;
    TempDir &operator=(TempDir &&) noexcept = default;

    TempDir()
    {
        auto tmpl = std::filesystem::temp_directory_path().string();
        tmpl.append("/linyaps-box-status-test-XXXXXX");

        auto *p = ::mkdtemp(tmpl.data());
        if (p == nullptr) {
            throw std::runtime_error(fmt::format("failed to create test dir: {}", errno));
        }
        path_ = p;
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return path_; }

private:
    std::filesystem::path path_;
};

// Mirrors the status.json written by the 2.2.x ll-box: it has a "status" field
// but no "process-start-time" key.
auto make_legacy_status_json(std::uint64_t pid, std::string_view id) -> nlohmann::json
{
    return nlohmann::json::object({ { "id", id },
                                    { "pid", pid },
                                    { "status", "running" },
                                    { "bundle", "/tmp/bundle" },
                                    { "created", "1787901642171875663" },
                                    { "owner", "dengbo" },
                                    { "annotations", nlohmann::json::object() },
                                    { "ociVersion", "1.2.1" } });
}

} // namespace

TEST(ContainerStatusJson, ParsesLegacyFormatWithoutStartTime)
{
    const auto j = make_legacy_status_json(1234, "legacy-id");

    container_status s;
    EXPECT_NO_THROW(s = j.get<container_status>());
    EXPECT_EQ(s.id, "legacy-id");
    EXPECT_EQ(s.pid, 1234);
    EXPECT_FALSE(s.process_start_time.has_value());
    EXPECT_EQ(s.oci_version, "1.2.1");
}

TEST(ContainerStatusJson, RoundTripPreservesStartTime)
{
    container_status s;
    s.id = "roundtrip";
    s.pid = 42;
    s.process_start_time = 987654;
    s.bundle = "/tmp/bundle";
    s.created = std::chrono::system_clock::now();
    s.owner = "dengbo";
    s.oci_version = "1.2.1";

    const auto j = nlohmann::json(s);
    const auto back = j.get<container_status>();

    ASSERT_TRUE(back.process_start_time.has_value());
    EXPECT_EQ(*back.process_start_time, 987654);
    EXPECT_EQ(back.pid, 42);
    EXPECT_EQ(back.owner, "dengbo");
}

TEST(CreatedTime, ParsesLegacyNanosecondsAndCurrentFormat)
{
    // Same instant: 2026-08-28T07:20:42.171875663Z.
    const auto legacy = linyaps_box::utils::from_created_time("1787901642171875663");
    const auto iso = linyaps_box::utils::from_created_time("2026-08-28T07:20:42.171875663Z");

    EXPECT_EQ(legacy, iso);
}

TEST(ContainerStatusDirectory, ReadsLegacyStatusFile)
{
    TempDir dir;
    std::ofstream out(dir.path() / "status.json");
    out << make_legacy_status_json(1234, "legacy-id").dump();
    out.close();

    const auto s = linyaps_box::status_directory(dir.path()).read();

    EXPECT_EQ(s.id, "legacy-id");
    EXPECT_FALSE(s.process_start_time.has_value());
}

TEST(DeriveStatus, UnknownStartTimeLiveProcessIsRunning)
{
    container_status s;
    s.pid = getpid();
    // process_start_time intentionally left nullopt, as with a legacy status file.

    EXPECT_EQ(linyaps_box::derive_status(s), runtime_status::RUNNING);
}

TEST(DeriveStatus, KnownStartTimeMismatchIsStopped)
{
    container_status s;
    s.pid = getpid();
    s.process_start_time = 0; // known but certainly wrong for any live process

    EXPECT_EQ(linyaps_box::derive_status(s), runtime_status::STOPPED);
}

TEST(DeriveStatus, DeadPidIsStopped)
{
    container_status s;
    s.pid = std::numeric_limits<pid_t>::max();
    s.process_start_time.reset();

    EXPECT_EQ(linyaps_box::derive_status(s), runtime_status::STOPPED);
}
