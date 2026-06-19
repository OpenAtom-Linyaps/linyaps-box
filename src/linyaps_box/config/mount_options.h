// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <sys/mount.h>

#include <array>
#include <sstream>
#include <string>
#include <string_view>

// clang-format off
#include "linyaps_box/config/kernel_fallbacks.h"
// clang-format on

namespace linyaps_box::config::mount_options {

struct flag_entry
{
    unsigned long value;
    std::string_view name;
};

// VFS mount(2) flags — passed directly to mount()'s 4th argument.
constexpr std::array<flag_entry, 20> vfs{ {
  { MS_BIND, "bind" },
  { MS_DIRSYNC, "dirsync" },
  { 0, "defaults" },
  { MS_I_VERSION, "iversion" },
  { MS_LAZYTIME, "lazytime" },
  { MS_MANDLOCK, "mand" },
  { MS_NOATIME, "noatime" },
  { MS_NODEV, "nodev" },
  { MS_NODIRATIME, "nodiratime" },
  { MS_NOEXEC, "noexec" },
  { MS_NOSUID, "nosuid" },
  { kernel::ms_nosymfollow, "nosymfollow" },
  { MS_BIND | MS_REC, "rbind" },
  { MS_RELATIME, "relatime" },
  { MS_REMOUNT, "remount" },
  { MS_RDONLY, "ro" },
  { MS_SILENT, "silent" },
  { MS_STRICTATIME, "strictatime" },
  { MS_SYNCHRONOUS, "sync" },
} };

// Negation options — clear the corresponding bit from VFS flags.
constexpr std::array<flag_entry, 14> unset{ {
  { MS_SYNCHRONOUS, "async" },
  { MS_NOATIME, "atime" },
  { MS_NODEV, "dev" },
  { MS_NODIRATIME, "diratime" },
  { MS_NOEXEC, "exec" },
  { MS_SILENT, "loud" },
  { MS_I_VERSION, "noiversion" },
  { MS_LAZYTIME, "nolazytime" },
  { MS_MANDLOCK, "nomand" },
  { MS_RELATIME, "norelatime" },
  { MS_STRICTATIME, "nostrictatime" },
  { MS_RDONLY, "rw" },
  { MS_NOSUID, "suid" },
  { kernel::ms_nosymfollow, "symfollow" },
} };

// Propagation flags — applied as separate mount(2) calls after the main mount.
constexpr std::array<flag_entry, 8> propagation{ {
  { MS_PRIVATE, "private" },
  { MS_PRIVATE | MS_REC, "rprivate" },
  { MS_SLAVE, "slave" },
  { MS_SLAVE | MS_REC, "rslave" },
  { MS_SHARED, "shared" },
  { MS_SHARED | MS_REC, "rshared" },
  { MS_UNBINDABLE, "unbindable" },
  { MS_UNBINDABLE | MS_REC, "runbindable" },
} };

// Recursive mount_setattr attributes — applied via mount_setattr(AT_RECURSIVE).
// Values use MOUNT_ATTR_* (kernel 5.12+).  These are consumed from mount options
// and NOT passed to mount(2).
//
// TODO: implement mount_setattr.  ATIME field handling: when any ATIME bit is
//       set/cleared, MOUNT_ATTR__ATIME must appear in attr_clr to reset the
//       field before applying the new mode.
constexpr std::array<flag_entry, 9> recursive_attr_set{ {
  { static_cast<unsigned long>(kernel::mount_attr_rdonly), "rro" },
  { static_cast<unsigned long>(kernel::mount_attr_nosuid), "rnosuid" },
  { static_cast<unsigned long>(kernel::mount_attr_nodev), "rnodev" },
  { static_cast<unsigned long>(kernel::mount_attr_noexec), "rnoexec" },
  { static_cast<unsigned long>(kernel::mount_attr_nodiratime), "rnodiratime" },
  { static_cast<unsigned long>(kernel::mount_attr_noatime), "rnoatime" },
  { static_cast<unsigned long>(kernel::mount_attr_strictatime), "rstrictatime" },
  { static_cast<unsigned long>(kernel::mount_attr_nosymfollow), "rnosymfollow" },
  { 0, "rrelatime" },
} };

constexpr std::array<flag_entry, 9> recursive_attr_clr{ {
  { static_cast<unsigned long>(kernel::mount_attr_rdonly), "rrw" },
  { static_cast<unsigned long>(kernel::mount_attr_nosuid), "rsuid" },
  { static_cast<unsigned long>(kernel::mount_attr_nodev), "rdev" },
  { static_cast<unsigned long>(kernel::mount_attr_noexec), "rexec" },
  { static_cast<unsigned long>(kernel::mount_attr_nodiratime), "rdiratime" },
  { static_cast<unsigned long>(kernel::mount_attr_noatime), "ratime" },
  { static_cast<unsigned long>(kernel::mount_attr_strictatime), "rnostrictatime" },
  { static_cast<unsigned long>(kernel::mount_attr_nosymfollow), "rsymfollow" },
  { 0, "rnorelatime" },
} };

template <size_t N>
constexpr const flag_entry *find(const std::array<flag_entry, N> &table,
                                 std::string_view key) noexcept
{
    for (const auto &entry : table) {
        if (entry.name == key) {
            return &entry;
        }
    }
    return nullptr;
}

// Dump VFS + propagation flags and any leftover unknown bits.
// Names are looked up from the vfs and propagation tables; unknown bits
// are printed as hex.
inline std::string dump(unsigned long vfs_flags, unsigned long prop_flags = 0) noexcept
{
    std::stringstream ss;
    ss << "[ ";

    // VFS flags
    for (const auto &[val, name] : vfs) {
        if (val != 0 && (vfs_flags & val) == val) {
            ss << name << " ";
            vfs_flags &= ~val;
        }
    }
    if (vfs_flags != 0) {
        ss << "0x" << std::hex << vfs_flags << std::dec << " ";
    }

    // Propagation flags
    if (prop_flags != 0) {
        ss << "prop:";
        for (const auto &[val, name] : propagation) {
            if ((prop_flags & val) == val) {
                ss << name << " ";
                prop_flags &= ~val;
            }
        }
        if (prop_flags != 0) {
            ss << "0x" << std::hex << prop_flags << std::dec << " ";
        }
    }

    ss << "]";
    return ss.str();
}

} // namespace linyaps_box::config::mount_options
