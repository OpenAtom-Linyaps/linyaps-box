// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace linyaps_box {

enum class cgroup_manager_t : std::uint16_t { disabled, systemd, cgroupfs };

struct cgroup_options
{
    std::unordered_map<std::string, std::string> annotations;
    std::filesystem::path cgroup_path;
    std::string id;
    std::filesystem::path state_root;
    pid_t pid;
    // resources and so on...
};

struct cgroup_status
{
    std::filesystem::path path;
    std::string scope;

    [[nodiscard]] cgroup_manager_t manager_type() const noexcept { return manager; }

private:
    friend class cgroup_manager;
    cgroup_manager_t manager;
};

inline std::ostream &operator<<(std::ostream &os, cgroup_manager_t manager)
{
    switch (manager) {
    case cgroup_manager_t::disabled: {
        os << "disabled";
    } break;
    case cgroup_manager_t::systemd: {
        os << "systemd";
    } break;
    case cgroup_manager_t::cgroupfs: {
        os << "cgroupfs";
    } break;
    default:
        os << "unknown";
    }

    return os;
}

} // namespace linyaps_box
