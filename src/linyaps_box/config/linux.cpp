// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/linux.h"

#include "linyaps_box/config/utils.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, linux &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "uidMappings")) {
            if (!val.is_null()) {
                val.get_to(v.uid_mappings.emplace());
            }
        } else if (key_matches(k, "gidMappings")) {
            if (!val.is_null()) {
                val.get_to(v.gid_mappings.emplace());
            }
        } else if (key_matches(k, "namespaces")) {
            if (!val.is_null()) {
                val.get_to(v.namespaces.emplace());
            }
        } else if (key_matches(k, "devices")) {
            if (!val.is_null()) {
                val.get_to(v.devices.emplace());
            }
        } else if (key_matches(k, "netDevices")) {
            if (!val.is_null()) {
                val.get_to(v.network_devices.emplace());
            }
        } else if (key_matches(k, "cgroupsPath")) {
            if (!val.is_null()) {
                val.get_to(v.cgroups_path.emplace());
            }
        } else if (key_matches(k, "maskedPaths")) {
            if (!val.is_null()) {
                val.get_to(v.masked_paths.emplace());
            }
        } else if (key_matches(k, "readonlyPaths")) {
            if (!val.is_null()) {
                val.get_to(v.readonly_paths.emplace());
            }
        } else if (key_matches(k, "mountLabel")) {
            if (!val.is_null()) {
                val.get_to(v.mount_label.emplace());
            }
        } else if (key_matches(k, "rootfsPropagation")) {
            if (!val.is_null()) {
                auto prop_name = val.get<std::string_view>();
                auto prop_opt = get_enum_table_from<rootfs_propagation>().from_name(prop_name);
                if (UNLIKELY(!prop_opt)) {
                    throw std::runtime_error(
                      fmt::format("unknown rootfsPropagation value: {}", prop_name));
                }

                v.rootfs_propagation_.emplace(*prop_opt);
            }
        } else if (key_matches(k, "sysctl")) {
            if (!val.is_null()) {
                val.get_to(v.sysctl.emplace());
            }
        } else if (key_matches(k, "timeOffsets")) {
            if (!val.is_null()) {
                auto &offsets = v.time_offsets.emplace();
                for (const auto &[clock_name, offset_json] : val.items()) {
                    offsets.emplace(clock_name, offset_json.get<time_offset>());
                }
            }
        } else if (key_matches(k, "personality")) {
            if (!val.is_null()) {
                val.get_to(v.personality_.emplace());
            }
        } else if (key_matches(k, "memoryPolicy")) {
            if (!val.is_null()) {
                val.get_to(v.memory_policy_.emplace());
            }
        } else if (key_matches(k, "intelRdt")) {
            if (!val.is_null()) {
                val.get_to(v.intel_rdt_.emplace());
            }
        } else if (key_matches(k, "resources")) {
            if (!val.is_null()) {
                val.get_to(v.resources_.emplace());
            }
        } else if (key_matches(k, "seccomp")) {
            if (!val.is_null()) {
                val.get_to(v.seccomp_.emplace());
            }
        }
    }
}

void validate(const linux &v)
{
    if (v.namespaces) {
        validate(*v.namespaces);
    }

    if ((v.uid_mappings || v.gid_mappings) && v.namespaces) {
        const auto userns_joined =
          std::any_of(v.namespaces->cbegin(), v.namespaces->cend(), [](const ns &entry) {
              return entry.type_ == ns::type::user && entry.path.has_value();
          });

        if (UNLIKELY(userns_joined)) {
            throw std::runtime_error(
              "linux.uidMappings/gidMappings cannot be used when the user namespace is "
              "joined via path");
        }
    }

    if (v.seccomp_) {
#ifdef LINYAPS_BOX_ENABLE_SECCOMP
        validate(*v.seccomp_);
#else
        throw std::runtime_error("seccomp support is not compiled in");
#endif
    }

    if (v.devices) {
        struct dev_node
        {
            int64_t major;
            int64_t minor;
            device::type type_;

            bool operator==(const dev_node &rhs) const noexcept
            {
                return type_ == rhs.type_ && major == rhs.major && minor == rhs.minor;
            }
        };

        // In the vast majority of cases, there aren't many devices in a container's device list
        std::vector<dev_node> seen;
        seen.reserve(v.devices->size());
        std::for_each(v.devices->cbegin(), v.devices->cend(), [&seen](const auto &device) {
            validate(device);

            const auto dev = dev_node{ device.major ? static_cast<int64_t>(*device.major) : -1,
                                       device.minor ? static_cast<int64_t>(*device.minor) : -1,
                                       device.type_ };
            if (UNLIKELY(std::find(seen.cbegin(), seen.cend(), dev) != seen.cend())) {
                throw std::runtime_error("duplicate device type/major/minor in linux.devices");
            }

            seen.emplace_back(dev);
        });
    }

    if (v.masked_paths) {
        std::for_each(v.masked_paths->cbegin(), v.masked_paths->cend(), [](const auto &p) {
            if (UNLIKELY(!p.is_absolute())) {
                throw std::runtime_error(
                  fmt::format("maskedPaths must be absolute paths, got: {}", p));
            }
        });
    }

    if (v.readonly_paths) {
        std::for_each(v.readonly_paths->cbegin(), v.readonly_paths->cend(), [](const auto &p) {
            if (UNLIKELY(!p.is_absolute())) {
                throw std::runtime_error(
                  fmt::format("readonlyPaths must be absolute paths, got: {}", p));
            }
        });
    }

    if (v.resources_) {
        validate(*v.resources_);
    }

    if (v.memory_policy_) {
        validate(*v.memory_policy_);
    }
}

} // namespace linyaps_box::config
