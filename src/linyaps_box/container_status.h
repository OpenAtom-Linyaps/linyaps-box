// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <sys/types.h>

namespace linyaps_box {

struct container_status
{
    std::unordered_map<std::string, std::string> annotations;
    std::filesystem::path bundle;
    std::string id;
    std::string oci_version;
    std::string owner; // extension field
    // Process identity from /proc/<pid>/stat, absent in legacy state files.
    std::optional<std::uint64_t> process_start_time;
    std::chrono::system_clock::time_point created; // extension field
    pid_t pid{ 0 };
};

enum class runtime_status : std::uint8_t { CREATING, CREATED, RUNNING, STOPPED };

auto to_string_view(runtime_status s) noexcept -> std::string_view;

auto to_json(nlohmann::json &j, const container_status &s) -> void;

[[nodiscard]] auto derive_status(const container_status &s) -> runtime_status;

auto to_oci_json(const container_status &s, runtime_status status) -> nlohmann::json;

auto ops_of(json::object_tag<container_status>) -> const json::frame_ops &;
} // namespace linyaps_box
