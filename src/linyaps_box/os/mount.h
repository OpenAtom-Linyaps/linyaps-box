// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <sys/mount.h>

#include <cstdint>

#include <sys/statvfs.h>

namespace linyaps_box::os::sys {

enum class mount_flag : std::uint32_t {
    none = 0,
    bind = MS_BIND,
    rec = MS_REC,
    ro = MS_RDONLY,
    nosuid = MS_NOSUID,
    nodev = MS_NODEV,
    noexec = MS_NOEXEC,
    noatime = MS_NOATIME,
    nodiratime = MS_NODIRATIME,
    relatime = MS_RELATIME,
    strictatime = MS_STRICTATIME,
    sync = MS_SYNCHRONOUS,
    dirsync = MS_DIRSYNC,
    iversion = MS_I_VERSION,
    lazytime = MS_LAZYTIME,
    mand = MS_MANDLOCK,
    silent = MS_SILENT,
    nosymfollow =
#ifdef MS_NOSYMFOLLOW
      MS_NOSYMFOLLOW
#else
      256U
#endif
    ,
    remount = MS_REMOUNT,
};
LINYAPS_ENABLE_BITMASK_ENUM(mount_flag)
LINYAPS_REGISTER_ENUM_TABLE(mount_flag,
                            19,
                            { mount_flag::none, "none" },
                            { mount_flag::bind, "MS_BIND" },
                            { mount_flag::rec, "MS_REC" },
                            { mount_flag::ro, "MS_RDONLY" },
                            { mount_flag::nosuid, "MS_NOSUID" },
                            { mount_flag::nodev, "MS_NODEV" },
                            { mount_flag::noexec, "MS_NOEXEC" },
                            { mount_flag::noatime, "MS_NOATIME" },
                            { mount_flag::nodiratime, "MS_NODIRATIME" },
                            { mount_flag::relatime, "MS_RELATIME" },
                            { mount_flag::strictatime, "MS_STRICTATIME" },
                            { mount_flag::sync, "MS_SYNCHRONOUS" },
                            { mount_flag::dirsync, "MS_DIRSYNC" },
                            { mount_flag::iversion, "MS_I_VERSION" },
                            { mount_flag::lazytime, "MS_LAZYTIME" },
                            { mount_flag::mand, "MS_MANDLOCK" },
                            { mount_flag::silent, "MS_SILENT" },
                            { mount_flag::nosymfollow, "MS_NOSYMFOLLOW" },
                            { mount_flag::remount, "MS_REMOUNT" })

enum class propagation_flag : std::uint32_t {
    none = 0,
    private_ = MS_PRIVATE,
    slave = MS_SLAVE,
    shared = MS_SHARED,
    unbindable = MS_UNBINDABLE,
    rec = MS_REC,
};
LINYAPS_ENABLE_BITMASK_ENUM(propagation_flag)
LINYAPS_REGISTER_ENUM_TABLE(propagation_flag,
                            6,
                            { propagation_flag::none, "none" },
                            { propagation_flag::private_, "MS_PRIVATE" },
                            { propagation_flag::slave, "MS_SLAVE" },
                            { propagation_flag::shared, "MS_SHARED" },
                            { propagation_flag::unbindable, "MS_UNBINDABLE" },
                            { propagation_flag::rec, "MS_REC" })

enum class statfs_flag : std::uint16_t {
    none = 0,
    rdonly = ST_RDONLY,
    nosuid = ST_NOSUID,
    nodev = ST_NODEV,
    noexec = ST_NOEXEC,
    synchronous = ST_SYNCHRONOUS,
    mandlock = ST_MANDLOCK,
    noatime = ST_NOATIME,
    nodiratime = ST_NODIRATIME,
    relatime = ST_RELATIME,
    nosymfollow =
#ifdef ST_NOSYMFOLLOW
      ST_NOSYMFOLLOW
#else
      0x2000
#endif
    ,
};
LINYAPS_ENABLE_BITMASK_ENUM(statfs_flag)
LINYAPS_REGISTER_ENUM_TABLE(statfs_flag,
                            11,
                            { statfs_flag::none, "none" },
                            { statfs_flag::rdonly, "ST_RDONLY" },
                            { statfs_flag::nosuid, "ST_NOSUID" },
                            { statfs_flag::nodev, "ST_NODEV" },
                            { statfs_flag::noexec, "ST_NOEXEC" },
                            { statfs_flag::synchronous, "ST_SYNCHRONOUS" },
                            { statfs_flag::mandlock, "ST_MANDLOCK" },
                            { statfs_flag::noatime, "ST_NOATIME" },
                            { statfs_flag::nodiratime, "ST_NODIRATIME" },
                            { statfs_flag::relatime, "ST_RELATIME" },
                            { statfs_flag::nosymfollow, "ST_NOSYMFOLLOW" })

[[nodiscard]] auto to_native(utils::bitflags<mount_flag> flags) noexcept -> unsigned long;

[[nodiscard]] auto to_native(utils::bitflags<propagation_flag> flags) noexcept -> unsigned long;

// Decode a statfs f_flags word.
// Some flags (ST_APPEND/ST_IMMUTABLE/ST_WRITE/ST_VALID) are dropped.
[[nodiscard]] auto decode_f_flags(unsigned long f_flags) noexcept -> utils::bitflags<statfs_flag>;

// st_flags -> the MS_* mount flags they correspond to.
[[nodiscard]] auto statfs_to_mount_flags(utils::bitflags<statfs_flag> flags) noexcept
  -> utils::bitflags<mount_flag>;

} // namespace linyaps_box::os::sys
