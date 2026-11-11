// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/linux.h"

#include "linyaps_box/json/parser.h"

#include <fmt/std.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(linux,
                          linux_schema,
                          "linux",
                          field<&linux::uid_mappings>("uidMappings").ignore_null(),
                          field<&linux::gid_mappings>("gidMappings").ignore_null(),
                          field<&linux::namespaces>("namespaces").ignore_null(),
                          field<&linux::devices>("devices").ignore_null(),
                          field<&linux::network_devices>("netDevices").ignore_null(),
                          field<&linux::cgroups_path>("cgroupsPath").ignore_null(),
                          field<&linux::rootfs_propagation_>("rootfsPropagation").ignore_null(),
                          field<&linux::sysctl>("sysctl").ignore_null(),
                          field<&linux::time_offsets>("timeOffsets").ignore_null(),
                          field<&linux::personality_>("personality").ignore_null(),
                          field<&linux::memory_policy_>("memoryPolicy").ignore_null(),
                          field<&linux::intel_rdt_>("intelRdt").ignore_null(),
                          field<&linux::resources_>("resources").ignore_null(),
                          field<&linux::seccomp_>("seccomp").ignore_null(),
                          field<&linux::masked_paths>("maskedPaths").ignore_null(),
                          field<&linux::readonly_paths>("readonlyPaths").ignore_null(),
                          field<&linux::mount_label>("mountLabel").ignore_null())

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

    if (v.intel_rdt_) {
        validate(*v.intel_rdt_);
    }

    if (v.time_offsets) {
        std::for_each(v.time_offsets->cbegin(), v.time_offsets->cend(), [](const auto &entry) {
            validate(entry.second);
        });
    }
}

} // namespace linyaps_box::config
