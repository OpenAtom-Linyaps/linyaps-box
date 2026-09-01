// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"
#include "linyaps_box/utils/enum_traits.h"

// rlimit types
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::process_t::rlimit_t::type_t,
  16,
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::AS, "RLIMIT_AS" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::CORE, "RLIMIT_CORE" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::CPU, "RLIMIT_CPU" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::DATA, "RLIMIT_DATA" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::FSIZE, "RLIMIT_FSIZE" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::LOCKS, "RLIMIT_LOCKS" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::MEMLOCK, "RLIMIT_MEMLOCK" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::MSGQUEUE, "RLIMIT_MSGQUEUE" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::NICE, "RLIMIT_NICE" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::NOFILE, "RLIMIT_NOFILE" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::NPROC, "RLIMIT_NPROC" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::RSS, "RLIMIT_RSS" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::RTPRIO, "RLIMIT_RTPRIO" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::RTTIME, "RLIMIT_RTTIME" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::SIGPENDING, "RLIMIT_SIGPENDING" },
  { linyaps_box::oci_config::process_t::rlimit_t::type_t::STACK, "RLIMIT_STACK" })

// scheduler policies
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::process_t::scheduler_t::policy_t,
  7,
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::OTHER, "SCHED_OTHER" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::FIFO, "SCHED_FIFO" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::RR, "SCHED_RR" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::BATCH, "SCHED_BATCH" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::ISO, "SCHED_ISO" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::IDLE, "SCHED_IDLE" },
  { linyaps_box::oci_config::process_t::scheduler_t::policy_t::DEADLINE, "SCHED_DEADLINE" })

// scheduler flags
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::process_t::scheduler_t::flag_t,
  7,
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::RESET_ON_FORK,
    "SCHED_FLAG_RESET_ON_FORK" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::RECLAIM, "SCHED_FLAG_RECLAIM" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::DL_OVERRUN, "SCHED_FLAG_DL_OVERRUN" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::KEEP_POLICY,
    "SCHED_FLAG_KEEP_POLICY" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::KEEP_PARAMS,
    "SCHED_FLAG_KEEP_PARAMS" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::UTIL_CLAMP_MIN,
    "SCHED_FLAG_UTIL_CLAMP_MIN" },
  { linyaps_box::oci_config::process_t::scheduler_t::flag_t::UTIL_CLAMP_MAX,
    "SCHED_FLAG_UTIL_CLAMP_MAX" })

// IO priority classes
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::process_t::io_priority_t::class_t,
  3,
  { linyaps_box::oci_config::process_t::io_priority_t::class_t::RT, "IOPRIO_CLASS_RT" },
  { linyaps_box::oci_config::process_t::io_priority_t::class_t::BEST_EFFORT, "IOPRIO_CLASS_BE" },
  { linyaps_box::oci_config::process_t::io_priority_t::class_t::IDLE, "IOPRIO_CLASS_IDLE" })

// personality domains
LINYAPS_REGISTER_ENUM_TABLE(linyaps_box::oci_config::linux_t::personality_t::domain_t,
                            2,
                            { linyaps_box::oci_config::linux_t::personality_t::domain_t::LINUX,
                              "LINUX" },
                            { linyaps_box::oci_config::linux_t::personality_t::domain_t::LINUX32,
                              "LINUX32" })

// memory policy modes
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::memory_policy_t::mode_t,
  7,
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::DEFAULT, "MPOL_DEFAULT" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::BIND, "MPOL_BIND" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::INTERLEAVE, "MPOL_INTERLEAVE" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::WEIGHTED_INTERLEAVE,
    "MPOL_WEIGHTED_INTERLEAVE" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::PREFERRED, "MPOL_PREFERRED" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::PREFERRED_MANY,
    "MPOL_PREFERRED_MANY" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::mode_t::LOCAL, "MPOL_LOCAL" })

// memory policy flags
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::memory_policy_t::flag_t,
  3,
  { linyaps_box::oci_config::linux_t::memory_policy_t::flag_t::NUMA_BALANCING,
    "MPOL_F_NUMA_BALANCING" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::flag_t::RELATIVE_NODES,
    "MPOL_F_RELATIVE_NODES" },
  { linyaps_box::oci_config::linux_t::memory_policy_t::flag_t::STATIC_NODES,
    "MPOL_F_STATIC_NODES" })

// seccomp operators
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t,
  7,
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::EQ, "SCMP_CMP_EQ" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::NE, "SCMP_CMP_NE" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LT, "SCMP_CMP_LT" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LE, "SCMP_CMP_LE" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GT, "SCMP_CMP_GT" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GE, "SCMP_CMP_GE" },
  { linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t::MASKED_EQ,
    "SCMP_CMP_MASKED_EQ" })

// seccomp actions
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::seccomp_t::action_t,
  9,
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::ALLOW, "SCMP_ACT_ALLOW" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::ERRNO, "SCMP_ACT_ERRNO" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::KILL, "SCMP_ACT_KILL" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::KILL_PROCESS, "SCMP_ACT_KILL_PROCESS" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::KILL_THREAD, "SCMP_ACT_KILL_THREAD" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::LOG, "SCMP_ACT_LOG" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::NOTIFY, "SCMP_ACT_NOTIFY" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::TRACE, "SCMP_ACT_TRACE" },
  { linyaps_box::oci_config::linux_t::seccomp_t::action_t::TRAP, "SCMP_ACT_TRAP" })

// seccomp flags
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::seccomp_t::flag_t,
  4,
  { linyaps_box::oci_config::linux_t::seccomp_t::flag_t::TSYNC, "SECCOMP_FILTER_FLAG_TSYNC" },
  { linyaps_box::oci_config::linux_t::seccomp_t::flag_t::LOG, "SECCOMP_FILTER_FLAG_LOG" },
  { linyaps_box::oci_config::linux_t::seccomp_t::flag_t::SPEC_ALLOW,
    "SECCOMP_FILTER_FLAG_SPEC_ALLOW" },
  { linyaps_box::oci_config::linux_t::seccomp_t::flag_t::WAIT_KILLABLE_RECV,
    "SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV" })

// mount extensions
LINYAPS_REGISTER_ENUM_TABLE(linyaps_box::oci_config::mount_t::extension,
                            2,
                            { linyaps_box::oci_config::mount_t::extension::COPY_SYMLINK,
                              "copy-symlink" },
                            { linyaps_box::oci_config::mount_t::extension::TMPCOPYUP, "tmpcopyup" })

// mount idmap types
LINYAPS_REGISTER_ENUM_TABLE(linyaps_box::oci_config::mount_t::idmap_type,
                            2,
                            { linyaps_box::oci_config::mount_t::idmap_type::IDMAP, "idmap" },
                            { linyaps_box::oci_config::mount_t::idmap_type::RIDMAP, "ridmap" })

// namespace types
LINYAPS_REGISTER_ENUM_TABLE(linyaps_box::oci_config::linux_t::namespace_t::type,
                            8,
                            { linyaps_box::oci_config::linux_t::namespace_t::type::IPC, "ipc" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::UTS, "uts" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::MOUNT, "mount" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::PID, "pid" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::NET, "network" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::USER, "user" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::CGROUP,
                              "cgroup" },
                            { linyaps_box::oci_config::linux_t::namespace_t::type::TIME, "time" })

// seccomp architectures
LINYAPS_REGISTER_ENUM_TABLE(
  linyaps_box::oci_config::linux_t::seccomp_t::arch_t,
  23,
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::X86, "SCMP_ARCH_X86" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::X86_64, "SCMP_ARCH_X86_64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::X32, "SCMP_ARCH_X32" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::ARM, "SCMP_ARCH_ARM" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::AARCH64, "SCMP_ARCH_AARCH64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPS, "SCMP_ARCH_MIPS" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPS64, "SCMP_ARCH_MIPS64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPS64N32, "SCMP_ARCH_MIPS64N32" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPSEL, "SCMP_ARCH_MIPSEL" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPSEL64, "SCMP_ARCH_MIPSEL64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::MIPSEL64N32, "SCMP_ARCH_MIPSEL64N32" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::PPC, "SCMP_ARCH_PPC" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::PPC64, "SCMP_ARCH_PPC64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::PPC64LE, "SCMP_ARCH_PPC64LE" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::S390, "SCMP_ARCH_S390" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::S390X, "SCMP_ARCH_S390X" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::PARISC, "SCMP_ARCH_PARISC" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::PARISC64, "SCMP_ARCH_PARISC64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::RISCV64, "SCMP_ARCH_RISCV64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::LOONGARCH64, "SCMP_ARCH_LOONGARCH64" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::M68K, "SCMP_ARCH_M68K" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::SH, "SCMP_ARCH_SH" },
  { linyaps_box::oci_config::linux_t::seccomp_t::arch_t::SHEB, "SCMP_ARCH_SHEB" })
