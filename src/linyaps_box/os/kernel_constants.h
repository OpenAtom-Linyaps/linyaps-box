// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// ATTENTION: This file must be the last one of including sequence

#pragma once

#include <sys/mount.h>

#include <cstdint>

namespace linyaps_box::os::sys {

// MS_NOSYMFOLLOW since Linux 5.10
#ifdef MS_NOSYMFOLLOW
inline constexpr unsigned int ms_nosymfollow = MS_NOSYMFOLLOW;
#else
inline constexpr unsigned int ms_nosymfollow = 256U;
#endif

// ST_NOSYMFOLLOW: statfs(2)/fstatfs(2) f_flags bit reported for a mount that
// has MS_NOSYMFOLLOW set. The kernel reports mount options in the ST_*
// namespace, so this is NOT the same value as MS_NOSYMFOLLOW (256); checking
// the MS_* value against f_flags would never match.
inline constexpr unsigned int st_nosymfollow = 0x2000U;

// MOUNT_ATTR_* for mount_setattr(2) since kernel 5.12
#ifdef MOUNT_ATTR_RDONLY
inline constexpr uint64_t mount_attr_rdonly = MOUNT_ATTR_RDONLY;
#else
inline constexpr uint64_t mount_attr_rdonly = 0x00000001ULL;
#endif

#ifdef MOUNT_ATTR_NOSUID
inline constexpr uint64_t mount_attr_nosuid = MOUNT_ATTR_NOSUID;
#else
inline constexpr uint64_t mount_attr_nosuid = 0x00000002ULL;
#endif

#ifdef MOUNT_ATTR_NODEV
inline constexpr uint64_t mount_attr_nodev = MOUNT_ATTR_NODEV;
#else
inline constexpr uint64_t mount_attr_nodev = 0x00000004ULL;
#endif

#ifdef MOUNT_ATTR_NOEXEC
inline constexpr uint64_t mount_attr_noexec = MOUNT_ATTR_NOEXEC;
#else
inline constexpr uint64_t mount_attr_noexec = 0x00000008ULL;
#endif

#ifdef MOUNT_ATTR_NOATIME
inline constexpr uint64_t mount_attr_noatime = MOUNT_ATTR_NOATIME;
#else
inline constexpr uint64_t mount_attr_noatime = 0x00000010ULL;
#endif

#ifdef MOUNT_ATTR_STRICTATIME
inline constexpr uint64_t mount_attr_strictatime = MOUNT_ATTR_STRICTATIME;
#else
inline constexpr uint64_t mount_attr_strictatime = 0x00000020ULL;
#endif

#ifdef MOUNT_ATTR_NODIRATIME
inline constexpr uint64_t mount_attr_nodiratime = MOUNT_ATTR_NODIRATIME;
#else
inline constexpr uint64_t mount_attr_nodiratime = 0x00000080ULL;
#endif

#ifdef MOUNT_ATTR_NOSYMFOLLOW
inline constexpr uint64_t mount_attr_nosymfollow = MOUNT_ATTR_NOSYMFOLLOW;
#else
inline constexpr uint64_t mount_attr_nosymfollow = 0x00200000ULL;
#endif

} // namespace linyaps_box::os::sys
