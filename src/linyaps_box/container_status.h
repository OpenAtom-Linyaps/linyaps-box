// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace linyaps_box {

struct container_status_t
{
    std::string ID;
    pid_t PID;

    enum class runtime_status : std::uint8_t { CREATING, CREATED, RUNNING, STOPPED } status;

    std::filesystem::path bundle;
    std::string created;
    uid_t owner;
    std::unordered_map<std::string, std::string> annotations;
};

} // namespace linyaps_box
