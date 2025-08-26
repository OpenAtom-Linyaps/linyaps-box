// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace linyaps_box {

enum class cgroup_manager_t : std::uint8_t { disabled, systemd, cgroupfs };

struct cgroup_options
{
    std::unordered_map<std::string, std::string> annotations;
    std::filesystem::path cgroup_path;
    std::filesystem::path state_root;
    std::string id;
    pid_t pid;
    // resources and so on...
};

struct cgroup_status
{
public:
    [[nodiscard]] auto path() const noexcept -> std::filesystem::path { return path_; }

    [[nodiscard]] auto scope() const noexcept -> std::string_view { return scope_; }

    [[nodiscard]] auto manager() const noexcept -> cgroup_manager_t { return manager_; }

private:
    friend class cgroup_manager;
    std::filesystem::path path_;
    std::string scope_;
    cgroup_manager_t manager_;
};

inline auto operator<<(std::ostream &stream, cgroup_manager_t manager) -> std::ostream &
{
    switch (manager) {
    case cgroup_manager_t::disabled: {
        stream << "disabled";
    } break;
    case cgroup_manager_t::systemd: {
        stream << "systemd";
    } break;
    case cgroup_manager_t::cgroupfs: {
        stream << "cgroupfs";
    } break;
    default: {
        stream << "unknown";
    } break;
    }

    return stream;
}

} // namespace linyaps_box
