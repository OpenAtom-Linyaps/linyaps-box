// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace linyaps_box {

struct container_status_t
{
    std::string ID;
    pid_t PID;

    std::string oci_version;
    enum class runtime_status : std::uint8_t { CREATING, CREATED, RUNNING, STOPPED };
    runtime_status status;
    std::filesystem::path bundle;
    std::string created; // extension field
    std::string owner;   // extension field
    std::unordered_map<std::string, std::string> annotations;
};

auto to_string_view(linyaps_box::container_status_t::runtime_status status) -> std::string_view;
auto from_string_view(std::string_view status) -> linyaps_box::container_status_t::runtime_status;
auto status_to_json(const linyaps_box::container_status_t &status) -> nlohmann::json;

} // namespace linyaps_box
