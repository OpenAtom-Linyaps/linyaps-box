// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Kernel UAPI constants that are missing from (or appear late in) the target
// toolchain headers.

// ATTENTION: only constants from the architecture-independent include/uapi/linux/*.h
// layer are collected here; architecture-dependent values never come from literals,
// syscall numbers live in os/syscall_nr.h and per-arch header macros (e.g. mips O_*)
// are always read from the platform header.
//
// Every constant is defined once with its frozen, never-renumbered UAPI
// value. When the platform header also defines the macro, a static_assert
// reconciles the two, so vendor header drift or macro pollution fails the
// build on that architecture instead of silently changing behaviour.
//
// Reconciliation activates per-macro: an assert is live only when the
// macro is already defined at that point. The includes below deliberately
// pull the platform headers that define them
//
// <sys/mount.h> for MS_* and (glibc >= 2.28) MOUNT_ATTR_*
// <sys/stat.h> for STATX_* via glibc's bits/statx.h which on __USE_GNU toolchains includes
// <linux/stat.h>.
//
//  On older toolchains (e.g. glibc 2.28 + kernel 4.19 headers) these macros are
// absent and the fallback constants below are the single source. This file cannot include the
// kernel UAPI headers itself: <linux/fs.h>/<linux/mount.h> collide with the MS_* enum of old glibc
// sys/mount.h.
//
// The static_asserts below are tripwires.
// Should a vendor ever renumber one of these frozen constants, do not weaken
// the assertion back to a header-following #ifdef and switch that constant to
// the os/syscall_nr.h pattern instead: probe the platform and select the value via full (partial)
// template specialization.

#pragma once

#include <linux/version.h>
#include <sys/mount.h>

#include <sys/stat.h>

namespace linyaps_box::os::sys {

// since Linux 5.10.
constexpr auto ms_nosymfollow = 256U;
#ifdef MS_NOSYMFOLLOW
static_assert(ms_nosymfollow == MS_NOSYMFOLLOW,
              "MS_NOSYMFOLLOW diverges from the frozen UAPI value");
#endif

// statfs/fstatfs f_flags bit reported for a mount that has MS_NOSYMFOLLOW set.
// The kernel reports mount options in the ST_* namespace, so this is NOT the
// same value as MS_NOSYMFOLLOW; checking the MS_* value against f_flags would
// never match. Not a UAPI macro, the literal below is the only source.
constexpr auto st_nosymfollow = 0x2000U;

// since kernel 5.12.
constexpr auto mount_attr_rdonly = 0x00000001ULL;
#ifdef MOUNT_ATTR_RDONLY
static_assert(mount_attr_rdonly == MOUNT_ATTR_RDONLY,
              "MOUNT_ATTR_RDONLY diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_nosuid = 0x00000002ULL;
#ifdef MOUNT_ATTR_NOSUID
static_assert(mount_attr_nosuid == MOUNT_ATTR_NOSUID,
              "MOUNT_ATTR_NOSUID diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_nodev = 0x00000004ULL;
#ifdef MOUNT_ATTR_NODEV
static_assert(mount_attr_nodev == MOUNT_ATTR_NODEV,
              "MOUNT_ATTR_NODEV diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_noexec = 0x00000008ULL;
#ifdef MOUNT_ATTR_NOEXEC
static_assert(mount_attr_noexec == MOUNT_ATTR_NOEXEC,
              "MOUNT_ATTR_NOEXEC diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_noatime = 0x00000010ULL;
#ifdef MOUNT_ATTR_NOATIME
static_assert(mount_attr_noatime == MOUNT_ATTR_NOATIME,
              "MOUNT_ATTR_NOATIME diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_strictatime = 0x00000020ULL;
#ifdef MOUNT_ATTR_STRICTATIME
static_assert(mount_attr_strictatime == MOUNT_ATTR_STRICTATIME,
              "MOUNT_ATTR_STRICTATIME diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_nodiratime = 0x00000080ULL;
#ifdef MOUNT_ATTR_NODIRATIME
static_assert(mount_attr_nodiratime == MOUNT_ATTR_NODIRATIME,
              "MOUNT_ATTR_NODIRATIME diverges from the frozen UAPI value");
#endif

constexpr auto mount_attr_nosymfollow = 0x00200000ULL;
#ifdef MOUNT_ATTR_NOSYMFOLLOW
static_assert(mount_attr_nosymfollow == MOUNT_ATTR_NOSYMFOLLOW,
              "MOUNT_ATTR_NOSYMFOLLOW diverges from the frozen UAPI value");
#endif

// since Linux 5.8.
constexpr auto statx_mnt_id = 0x00001000U;
#ifdef STATX_MNT_ID
static_assert(statx_mnt_id == STATX_MNT_ID, "STATX_MNT_ID diverges from the frozen UAPI value");
#endif

// since Linux 6.8.
constexpr auto statx_mnt_id_unique = 0x00004000U;
#ifdef STATX_MNT_ID_UNIQUE
static_assert(statx_mnt_id_unique == STATX_MNT_ID_UNIQUE,
              "STATX_MNT_ID_UNIQUE diverges from the frozen UAPI value");
#endif

// Inode number of the procfs root directory
constexpr auto proc_root_ino = 1U;
// PROCFS_ROOT_INO is a UAPI enum (not a macro), so #ifdef cannot detect its
// availability; gate on the header version instead (the UAPI export landed in
// Linux 6.17).  <linux/fs.h> itself is only includable where the header
// version is new enough, the MS_* macros it carried collide with the MS_*
// enum of old glibc (2.28) sys/mount.h.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
#  include <linux/fs.h>
static_assert(proc_root_ino == PROCFS_ROOT_INO,
              "PROCFS_ROOT_INO diverges from the frozen UAPI value");
#endif

} // namespace linyaps_box::os::sys
