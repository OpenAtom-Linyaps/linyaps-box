// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/device.h"
#include "linyaps_box/config/id_mapping.h"
#include "linyaps_box/config/intel_rdt.h"
#include "linyaps_box/config/memory_policy.h"
#include "linyaps_box/config/network_device.h"
#include "linyaps_box/config/ns.h"
#include "linyaps_box/config/personality.h"
#include "linyaps_box/config/resources.h"
#include "linyaps_box/config/seccomp.h"
#include "linyaps_box/config/time_offset.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace linyaps_box::config {

enum class rootfs_propagation : std::uint8_t { private_, shared, slave, unbindable };

LINYAPS_REGISTER_ENUM_TABLE(rootfs_propagation,
                            4,
                            { rootfs_propagation::private_, "private" },
                            { rootfs_propagation::shared, "shared" },
                            { rootfs_propagation::slave, "slave" },
                            { rootfs_propagation::unbindable, "unbindable" })

struct linux
{
    std::optional<std::vector<ns>> namespaces;
    std::optional<std::vector<id_mapping>> uid_mappings;
    std::optional<std::vector<id_mapping>> gid_mappings;
    std::optional<std::unordered_map<std::string, time_offset>> time_offsets;
    std::optional<std::vector<device>> devices;
    std::optional<std::unordered_map<std::string, network_device>> network_devices;
    std::optional<std::string> cgroups_path;
    std::optional<resources> resources_;
    std::optional<intel_rdt> intel_rdt_;
    std::optional<memory_policy> memory_policy_;
    std::optional<std::unordered_map<std::string, std::string>> sysctl;
    std::optional<seccomp> seccomp_;
    std::optional<std::vector<std::filesystem::path>> masked_paths;
    std::optional<std::vector<std::filesystem::path>> readonly_paths;
    std::optional<std::string> mount_label;
    std::optional<personality> personality_;
    std::optional<rootfs_propagation> rootfs_propagation_;
};

void from_json(const nlohmann::json &j, linux &v);

void validate(const linux &v);

} // namespace linyaps_box::config
