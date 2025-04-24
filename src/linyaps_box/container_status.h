// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
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

std::string to_string(linyaps_box::container_status_t::runtime_status status);
linyaps_box::container_status_t::runtime_status from_string(std::string_view status);
nlohmann::json status_to_json(const linyaps_box::container_status_t &status);

} // namespace linyaps_box
