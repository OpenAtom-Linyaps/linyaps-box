// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/specconv/mount.h"

#include "linyaps_box/utils/utils.h"

#include <array>

namespace linyaps_box::specconv {

using namespace linyaps_box::config;

namespace {

struct mount_flag_mapping
{
    vfs_flag config;
    os::sys::mount_flag os;
};

constexpr auto mount_flag_mappings = std::array{
    mount_flag_mapping{ vfs_flag::bind, os::sys::mount_flag::bind },
    mount_flag_mapping{ vfs_flag::rec, os::sys::mount_flag::rec },
    mount_flag_mapping{ vfs_flag::ro, os::sys::mount_flag::ro },
    mount_flag_mapping{ vfs_flag::nosuid, os::sys::mount_flag::nosuid },
    mount_flag_mapping{ vfs_flag::nodev, os::sys::mount_flag::nodev },
    mount_flag_mapping{ vfs_flag::noexec, os::sys::mount_flag::noexec },
    mount_flag_mapping{ vfs_flag::noatime, os::sys::mount_flag::noatime },
    mount_flag_mapping{ vfs_flag::nodiratime, os::sys::mount_flag::nodiratime },
    mount_flag_mapping{ vfs_flag::relatime, os::sys::mount_flag::relatime },
    mount_flag_mapping{ vfs_flag::strictatime, os::sys::mount_flag::strictatime },
    mount_flag_mapping{ vfs_flag::sync, os::sys::mount_flag::sync },
    mount_flag_mapping{ vfs_flag::dirsync, os::sys::mount_flag::dirsync },
    mount_flag_mapping{ vfs_flag::iversion, os::sys::mount_flag::iversion },
    mount_flag_mapping{ vfs_flag::lazytime, os::sys::mount_flag::lazytime },
    mount_flag_mapping{ vfs_flag::mand, os::sys::mount_flag::mand },
    mount_flag_mapping{ vfs_flag::silent, os::sys::mount_flag::silent },
    mount_flag_mapping{ vfs_flag::nosymfollow, os::sys::mount_flag::nosymfollow },
    mount_flag_mapping{ vfs_flag::remount, os::sys::mount_flag::remount },
};

struct propagation_flag_mapping
{
    propagation_flag config;
    os::sys::propagation_flag os;
};

constexpr auto propagation_flag_mappings = std::array{
    propagation_flag_mapping{ propagation_flag::private_, os::sys::propagation_flag::private_ },
    propagation_flag_mapping{ propagation_flag::slave, os::sys::propagation_flag::slave },
    propagation_flag_mapping{ propagation_flag::shared, os::sys::propagation_flag::shared },
    propagation_flag_mapping{ propagation_flag::unbindable, os::sys::propagation_flag::unbindable },
    propagation_flag_mapping{ propagation_flag::rec, os::sys::propagation_flag::rec },
};

[[nodiscard]] constexpr auto mapped_vfs_flags() noexcept -> utils::bitflags<vfs_flag>
{
    utils::bitflags<vfs_flag> mask{ };
    for (const auto &entry : mount_flag_mappings) {
        mask |= entry.config;
    }
    return mask;
}

[[nodiscard]] constexpr auto mapped_propagation_flags() noexcept
  -> utils::bitflags<propagation_flag>
{
    utils::bitflags<propagation_flag> mask{ };
    for (const auto &entry : propagation_flag_mappings) {
        mask |= entry.config;
    }
    return mask;
}

static_assert(mapped_vfs_flags() == utils::bitflags<vfs_flag>::all(),
              "every vfs_flag needs an OS mapping in mount_flag_mappings");
static_assert(mapped_propagation_flags() == utils::bitflags<propagation_flag>::all(),
              "every propagation_flag needs an OS mapping in propagation_flag_mappings");

} // namespace

auto to_mount_flags(utils::bitflags<vfs_flag> flags) noexcept
  -> utils::bitflags<os::sys::mount_flag>
{
    utils::bitflags<os::sys::mount_flag> out{ };
    for (const auto &entry : mount_flag_mappings) {
        if (flags.contains(entry.config)) {
            out |= entry.os;
        }
    }
    return out;
}

auto from_mount_flags(utils::bitflags<os::sys::mount_flag> flags) noexcept
  -> utils::bitflags<vfs_flag>
{
    utils::bitflags<vfs_flag> out{ };
    for (const auto &entry : mount_flag_mappings) {
        if (flags.contains(entry.os)) {
            out |= entry.config;
        }
    }
    return out;
}

auto to_propagation(utils::bitflags<propagation_flag> flags) noexcept
  -> utils::bitflags<os::sys::propagation_flag>
{
    utils::bitflags<os::sys::propagation_flag> out{ };
    for (const auto &entry : propagation_flag_mappings) {
        if (flags.contains(entry.config)) {
            out |= entry.os;
        }
    }
    return out;
}

auto to_rootfs_propagation(rootfs_propagation prop) noexcept
  -> utils::bitflags<os::sys::propagation_flag>
{
    switch (prop) {
    case rootfs_propagation::private_:
        return os::sys::propagation_flag::private_;
    case rootfs_propagation::shared:
        return os::sys::propagation_flag::shared;
    case rootfs_propagation::slave:
        return os::sys::propagation_flag::slave;
    case rootfs_propagation::unbindable:
        return os::sys::propagation_flag::unbindable;
    }

    utils::unreachable("unknown rootfs propagation type");
}

} // namespace linyaps_box::specconv
