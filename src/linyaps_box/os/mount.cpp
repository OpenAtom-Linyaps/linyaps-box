// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/mount.h"

#include <array>
#include <utility>

namespace linyaps_box::os::sys {

auto to_native(utils::bitflags<mount_flag> flags) noexcept -> unsigned long
{
    return static_cast<unsigned long>(flags.to_raw());
}

auto to_native(utils::bitflags<propagation_flag> flags) noexcept -> unsigned long
{
    return static_cast<unsigned long>(flags.to_raw());
}

auto decode_f_flags(unsigned long f_flags) noexcept -> utils::bitflags<statfs_flag>
{
    return utils::bitflags<statfs_flag>::from_raw_truncate(static_cast<std::uint32_t>(f_flags));
}

auto statfs_to_mount_flags(utils::bitflags<statfs_flag> flags) noexcept
  -> utils::bitflags<mount_flag>
{
    constexpr auto mappings = std::array{
        std::pair{ statfs_flag::rdonly, mount_flag::ro },
        std::pair{ statfs_flag::nosuid, mount_flag::nosuid },
        std::pair{ statfs_flag::nodev, mount_flag::nodev },
        std::pair{ statfs_flag::noexec, mount_flag::noexec },
        std::pair{ statfs_flag::synchronous, mount_flag::sync },
        std::pair{ statfs_flag::mandlock, mount_flag::mand },
        std::pair{ statfs_flag::noatime, mount_flag::noatime },
        std::pair{ statfs_flag::nodiratime, mount_flag::nodiratime },
        std::pair{ statfs_flag::relatime, mount_flag::relatime },
        std::pair{ statfs_flag::nosymfollow, mount_flag::nosymfollow },
    };

    utils::bitflags<mount_flag> out{ };
    for (const auto &[st, ms] : mappings) {
        if (flags.contains(st)) {
            out |= ms;
        }
    }

    // There is no ST_ bit for strictatime: it is the mode when neither noatime
    // nor relatime is set.
    if (!out.intersects(mount_flag::noatime | mount_flag::relatime)) {
        out |= mount_flag::strictatime;
    }

    return out;
}

} // namespace linyaps_box::os::sys
