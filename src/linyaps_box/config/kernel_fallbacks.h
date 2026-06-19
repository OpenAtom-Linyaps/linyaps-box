// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// ATTENTION: This file must be the last one of including sequence

#pragma once

#include <cstdint>

namespace linyaps_box::kernel {

// CLONE_NEWTIME since Linux 5.6
#ifdef CLONE_NEWTIME
inline constexpr unsigned int clone_newtime = CLONE_NEWTIME;
#else
inline constexpr unsigned int clone_newtime = 0x00000080;
#endif

// scheduler flags — CLAMP_MIN/CLAMP_MAX since Linux 5.3
#ifdef SCHED_FLAG_UTIL_CLAMP_MIN
inline constexpr unsigned int sched_flag_util_clamp_min = SCHED_FLAG_UTIL_CLAMP_MIN;
#else
inline constexpr unsigned int sched_flag_util_clamp_min = 0x20;
#endif

#ifdef SCHED_FLAG_UTIL_CLAMP_MAX
inline constexpr unsigned int sched_flag_util_clamp_max = SCHED_FLAG_UTIL_CLAMP_MAX;
#else
inline constexpr unsigned int sched_flag_util_clamp_max = 0x40;
#endif

// SCHED_ISO is not standard — use value 4
#ifdef SCHED_ISO
inline constexpr int sched_iso = SCHED_ISO;
#else
inline constexpr int sched_iso = 4;
#endif

// memory policy modes — PREFERRED_MANY since 5.15, WEIGHTED_INTERLEAVE since 6.9
#ifdef MPOL_PREFERRED_MANY
inline constexpr int mpol_preferred_many = MPOL_PREFERRED_MANY;
#else
inline constexpr int mpol_preferred_many = 5;
#endif

#ifdef MPOL_WEIGHTED_INTERLEAVE
inline constexpr int mpol_weighted_interleave = MPOL_WEIGHTED_INTERLEAVE;
#else
inline constexpr int mpol_weighted_interleave = 6;
#endif

// memory policy flags — NUMA_BALANCING since 5.12
#ifdef MPOL_F_NUMA_BALANCING
inline constexpr unsigned int mpol_f_numa_balancing = MPOL_F_NUMA_BALANCING;
#else
inline constexpr unsigned int mpol_f_numa_balancing = (1U << 13);
#endif

// seccomp filter flags — LOG/SPEC_ALLOW since 4.8, WAIT_KILLABLE_RECV since 5.6
#ifdef SECCOMP_FILTER_FLAG_LOG
inline constexpr unsigned long seccomp_filter_flag_log = SECCOMP_FILTER_FLAG_LOG;
#else
inline constexpr unsigned long seccomp_filter_flag_log = (1UL << 1);
#endif

#ifdef SECCOMP_FILTER_FLAG_SPEC_ALLOW
inline constexpr unsigned long seccomp_filter_flag_spec_allow = SECCOMP_FILTER_FLAG_SPEC_ALLOW;
#else
inline constexpr unsigned long seccomp_filter_flag_spec_allow = (1UL << 2);
#endif

#ifdef SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV
inline constexpr unsigned long seccomp_filter_flag_wait_killable_recv =
  SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV;
#else
inline constexpr unsigned long seccomp_filter_flag_wait_killable_recv = (1UL << 5);
#endif

// seccomp actions — KILL_PROCESS/KILL_THREAD since 4.14, LOG since 4.14, NOTIFY since 5.0
#ifdef SCMP_ACT_KILL_PROCESS
inline constexpr unsigned int scmp_act_kill_process = SCMP_ACT_KILL_PROCESS;
#else
inline constexpr unsigned int scmp_act_kill_process = 0x80000000U;
#endif

#ifdef SCMP_ACT_KILL_THREAD
inline constexpr unsigned int scmp_act_kill_thread = SCMP_ACT_KILL_THREAD;
#else
inline constexpr unsigned int scmp_act_kill_thread = 0x00000000U;
#endif

#ifdef SCMP_ACT_LOG
inline constexpr unsigned int scmp_act_log = SCMP_ACT_LOG;
#else
inline constexpr unsigned int scmp_act_log = 0x7ffc0000U;
#endif

#ifdef SCMP_ACT_NOTIFY
inline constexpr unsigned int scmp_act_notify = SCMP_ACT_NOTIFY;
#else
inline constexpr unsigned int scmp_act_notify = 0x7fc00000U;
#endif

// MS_NOSYMFOLLOW since Linux 5.10
#ifdef MS_NOSYMFOLLOW
inline constexpr unsigned int ms_nosymfollow = MS_NOSYMFOLLOW;
#else
inline constexpr unsigned int ms_nosymfollow = 256U;
#endif

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

} // namespace linyaps_box::kernel
