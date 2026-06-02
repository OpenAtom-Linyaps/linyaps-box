// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config.h"

#include "linyaps_box/utils/semver.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <sstream>

#include <sys/resource.h>

namespace nlohmann {
template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json &j, const std::optional<T> &opt)
    {
        if (opt) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }

    static void from_json(const json &j, std::optional<T> &opt)
    {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.get<T>();
        }
    }
};
} // namespace nlohmann

namespace {

template <typename T>
const T *find_in_sorted(const std::vector<std::pair<std::string_view, T>> &vec,
                        std::string_view key) noexcept
{
    auto it =
      std::lower_bound(vec.begin(), vec.end(), key, [](const auto &p, std::string_view k) noexcept {
          return p.first < k;
      });
    if (it != vec.end() && it->first == key) {
        return &it->second;
    }
    return nullptr;
}

// Maps are sorted vectors for cache-friendly small-N lookups.
// N ≤ 22 for all maps; linear / binary search beats std::unordered_map at this scale.

static const std::vector<std::pair<std::string_view, unsigned long>> propagation_flags_map{
    { "private", MS_PRIVATE },
    { "rprivate", MS_PRIVATE | MS_REC },
    { "rslave", MS_SLAVE | MS_REC },
    { "runbindable", MS_UNBINDABLE | MS_REC },
    { "rshared", MS_SHARED | MS_REC },
    { "shared", MS_SHARED },
    { "slave", MS_SLAVE },
    { "unbindable", MS_UNBINDABLE },
};

static const std::vector<std::pair<std::string_view, unsigned long>> flags_map{
    { "bind", MS_BIND },
    { "defaults", 0 },
    { "dirsync", MS_DIRSYNC },
    { "iversion", MS_I_VERSION },
    { "lazytime", MS_LAZYTIME },
    { "mand", MS_MANDLOCK },
    { "noatime", MS_NOATIME },
    { "nodev", MS_NODEV },
    { "nodiratime", MS_NODIRATIME },
    { "noexec", MS_NOEXEC },
    { "nosuid", MS_NOSUID },
    { "nosymfollow", LINGYAPS_MS_NOSYMFOLLOW },
    { "rbind", MS_BIND | MS_REC },
    { "relatime", MS_RELATIME },
    { "remount", MS_REMOUNT },
    { "ro", MS_RDONLY },
    { "silent", MS_SILENT },
    { "strictatime", MS_STRICTATIME },
    { "sync", MS_SYNCHRONOUS },
};

static const std::vector<std::pair<std::string_view, unsigned long>> unset_flags_map{
    { "async", MS_SYNCHRONOUS },
    { "atime", MS_NOATIME },
    { "dev", MS_NODEV },
    { "diratime", MS_NODIRATIME },
    { "exec", MS_NOEXEC },
    { "loud", MS_SILENT },
    { "noiversion", MS_I_VERSION },
    { "nolazytime", MS_LAZYTIME },
    { "nomand", MS_MANDLOCK },
    { "norelatime", MS_RELATIME },
    { "nostrictatime", MS_STRICTATIME },
    { "rw", MS_RDONLY },
    { "suid", MS_NOSUID },
    { "symfollow", LINGYAPS_MS_NOSYMFOLLOW },
};

static const std::vector<std::pair<std::string_view, linyaps_box::config::mount_t::extension>>
  extra_flags_map{
      { "copy-symlink", linyaps_box::config::mount_t::extension::COPY_SYMLINK },
  };

// For serialization: list in priority order (r* before non-recursive)
static const std::vector<std::pair<unsigned long, std::string_view>> propagation_flags_to_names{
    { MS_PRIVATE | MS_REC, "rprivate" },       { MS_PRIVATE, "private" },
    { MS_SLAVE | MS_REC, "rslave" },           { MS_SLAVE, "slave" },
    { MS_SHARED | MS_REC, "rshared" },         { MS_SHARED, "shared" },
    { MS_UNBINDABLE | MS_REC, "runbindable" }, { MS_UNBINDABLE, "unbindable" },
};

static const std::vector<std::pair<unsigned long, std::string_view>> flags_to_names{
    { MS_BIND | MS_REC, "rbind" },
    { MS_BIND, "bind" },
    { MS_DIRSYNC, "dirsync" },
    { MS_I_VERSION, "iversion" },
    { MS_LAZYTIME, "lazytime" },
    { MS_MANDLOCK, "mand" },
    { MS_NOATIME, "noatime" },
    { MS_NODEV, "nodev" },
    { MS_NODIRATIME, "nodiratime" },
    { MS_NOEXEC, "noexec" },
    { MS_NOSUID, "nosuid" },
    { LINGYAPS_MS_NOSYMFOLLOW, "nosymfollow" },
    { MS_RDONLY, "ro" },
    { MS_RELATIME, "relatime" },
    { MS_REMOUNT, "remount" },
    { MS_SILENT, "silent" },
    { MS_STRICTATIME, "strictatime" },
    { MS_SYNCHRONOUS, "sync" },
};

static const std::vector<std::pair<linyaps_box::config::mount_t::extension, std::string_view>>
  extra_flags_to_names{
      { linyaps_box::config::mount_t::extension::COPY_SYMLINK, "copy-symlink" },
  };

auto parse_mount_options(const std::vector<std::string> &options)
  -> std::tuple<unsigned long, unsigned long, linyaps_box::config::mount_t::extension, std::string>
{
    unsigned long flags = 0;
    auto extra_flags = linyaps_box::config::mount_t::extension::NONE;
    unsigned long propagation_flags = 0;
    std::string data;

    for (const auto &opt : options) {
        if (auto *val = find_in_sorted(flags_map, opt)) {
            flags |= *val;
            continue;
        }
        if (auto *val = find_in_sorted(unset_flags_map, opt)) {
            flags &= ~*val;
            continue;
        }
        if (auto *val = find_in_sorted(propagation_flags_map, opt)) {
            propagation_flags |= *val;
            continue;
        }
        if (auto *val = find_in_sorted(extra_flags_map, opt)) {
            extra_flags |= *val;
            continue;
        }

        if (!data.empty()) {
            data += ',';
        }
        data += opt;
    }

    return { flags, propagation_flags, extra_flags, std::move(data) };
}

auto to_mount_options(unsigned long flags,
                      unsigned long propagation_flags,
                      linyaps_box::config::mount_t::extension ext_flags,
                      const std::string &data) -> std::vector<std::string>
{
    std::vector<std::string> result;

    for (const auto &[val, name] : flags_to_names) {
        if ((flags & val) == val) {
            result.emplace_back(name);
            flags &= ~val;
        }
    }
    for (const auto &[val, name] : propagation_flags_to_names) {
        if ((propagation_flags & val) == val) {
            result.emplace_back(name);
            propagation_flags &= ~val;
        }
    }
    for (const auto &[val, name] : extra_flags_to_names) {
        if ((static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(ext_flags)
             & static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(val))
            == static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(val)) {
            result.emplace_back(name);
        }
    }

    if (!data.empty()) {
        result.push_back(data);
    }

    return result;
}

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::namespace_t::type>>
  namespace_type_map{
      { "cgroup", linyaps_box::config::linux_t::namespace_t::type::CGROUP },
      { "ipc", linyaps_box::config::linux_t::namespace_t::type::IPC },
      { "mount", linyaps_box::config::linux_t::namespace_t::type::MOUNT },
      { "network", linyaps_box::config::linux_t::namespace_t::type::NET },
      { "pid", linyaps_box::config::linux_t::namespace_t::type::PID },
      { "time", linyaps_box::config::linux_t::namespace_t::type::TIME },
      { "user", linyaps_box::config::linux_t::namespace_t::type::USER },
      { "uts", linyaps_box::config::linux_t::namespace_t::type::UTS },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::namespace_t::type, std::string_view>>
  namespace_type_to_string{
      { linyaps_box::config::linux_t::namespace_t::type::TIME, "time" },
      { linyaps_box::config::linux_t::namespace_t::type::MOUNT, "mount" },
      { linyaps_box::config::linux_t::namespace_t::type::CGROUP, "cgroup" },
      { linyaps_box::config::linux_t::namespace_t::type::UTS, "uts" },
      { linyaps_box::config::linux_t::namespace_t::type::IPC, "ipc" },
      { linyaps_box::config::linux_t::namespace_t::type::USER, "user" },
      { linyaps_box::config::linux_t::namespace_t::type::PID, "pid" },
      { linyaps_box::config::linux_t::namespace_t::type::NET, "network" },
  };

static const std::vector<std::pair<std::string_view, unsigned int>> rootfs_propagation_map{
    { "private", MS_PRIVATE },
    { "shared", MS_SHARED },
    { "slave", MS_SLAVE },
    { "unbindable", MS_UNBINDABLE },
};

static const std::vector<std::pair<unsigned int, std::string_view>> rootfs_propagation_to_string{
    { MS_PRIVATE, "private" },
    { MS_SLAVE, "slave" },
    { MS_SHARED, "shared" },
    { MS_UNBINDABLE, "unbindable" },
};

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::process_t::scheduler_t::policy_t>>
  scheduler_policy_map{
      { "SCHED_BATCH", linyaps_box::config::process_t::scheduler_t::policy_t::BATCH },
      { "SCHED_DEADLINE", linyaps_box::config::process_t::scheduler_t::policy_t::DEADLINE },
      { "SCHED_FIFO", linyaps_box::config::process_t::scheduler_t::policy_t::FIFO },
      { "SCHED_IDLE", linyaps_box::config::process_t::scheduler_t::policy_t::IDLE },
      { "SCHED_ISO", linyaps_box::config::process_t::scheduler_t::policy_t::ISO },
      { "SCHED_OTHER", linyaps_box::config::process_t::scheduler_t::policy_t::OTHER },
      { "SCHED_RR", linyaps_box::config::process_t::scheduler_t::policy_t::RR },
  };

static const std::vector<
  std::pair<linyaps_box::config::process_t::scheduler_t::policy_t, std::string_view>>
  scheduler_policy_to_string{
      { linyaps_box::config::process_t::scheduler_t::policy_t::OTHER, "SCHED_OTHER" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::FIFO, "SCHED_FIFO" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::RR, "SCHED_RR" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::BATCH, "SCHED_BATCH" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::ISO, "SCHED_ISO" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::IDLE, "SCHED_IDLE" },
      { linyaps_box::config::process_t::scheduler_t::policy_t::DEADLINE, "SCHED_DEADLINE" },
  };

static const std::vector<std::pair<int, std::string_view>> scheduler_flags_to_names{
    { SCHED_FLAG_RESET_ON_FORK, "SCHED_FLAG_RESET_ON_FORK" },
    { SCHED_FLAG_RECLAIM, "SCHED_FLAG_RECLAIM" },
    { SCHED_FLAG_DL_OVERRUN, "SCHED_FLAG_DL_OVERRUN" },
    { SCHED_FLAG_KEEP_POLICY, "SCHED_FLAG_KEEP_POLICY" },
    { SCHED_FLAG_KEEP_PARAMS, "SCHED_FLAG_KEEP_PARAMS" },
    { SCHED_FLAG_UTIL_CLAMP_MIN, "SCHED_FLAG_UTIL_CLAMP_MIN" },
    { SCHED_FLAG_UTIL_CLAMP_MAX, "SCHED_FLAG_UTIL_CLAMP_MAX" },
};

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::process_t::io_priority_t::class_t>>
  io_priority_class_map{
      { "IOPRIO_CLASS_BE", linyaps_box::config::process_t::io_priority_t::class_t::BEST_EFFORT },
      { "IOPRIO_CLASS_IDLE", linyaps_box::config::process_t::io_priority_t::class_t::IDLE },
      { "IOPRIO_CLASS_RT", linyaps_box::config::process_t::io_priority_t::class_t::RT },
  };

static const std::vector<
  std::pair<linyaps_box::config::process_t::io_priority_t::class_t, std::string_view>>
  io_priority_class_to_string{
      { linyaps_box::config::process_t::io_priority_t::class_t::RT, "IOPRIO_CLASS_RT" },
      { linyaps_box::config::process_t::io_priority_t::class_t::BEST_EFFORT, "IOPRIO_CLASS_BE" },
      { linyaps_box::config::process_t::io_priority_t::class_t::IDLE, "IOPRIO_CLASS_IDLE" },
  };

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::personality_t::domain_t>>
  personality_domain_map{
      { "LINUX", linyaps_box::config::linux_t::personality_t::domain_t::LINUX },
      { "LINUX32", linyaps_box::config::linux_t::personality_t::domain_t::LINUX32 },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::personality_t::domain_t, std::string_view>>
  personality_domain_to_string{
      { linyaps_box::config::linux_t::personality_t::domain_t::LINUX, "LINUX" },
      { linyaps_box::config::linux_t::personality_t::domain_t::LINUX32, "LINUX32" },
  };

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::memory_policy_t::mode_t>>
  memory_policy_mode_map{
      { "MPOL_BIND", linyaps_box::config::linux_t::memory_policy_t::mode_t::BIND },
      { "MPOL_DEFAULT", linyaps_box::config::linux_t::memory_policy_t::mode_t::DEFAULT },
      { "MPOL_INTERLEAVE", linyaps_box::config::linux_t::memory_policy_t::mode_t::INTERLEAVE },
      { "MPOL_LOCAL", linyaps_box::config::linux_t::memory_policy_t::mode_t::LOCAL },
      { "MPOL_PREFERRED", linyaps_box::config::linux_t::memory_policy_t::mode_t::PREFERRED },
      { "MPOL_PREFERRED_MANY",
        linyaps_box::config::linux_t::memory_policy_t::mode_t::PREFERRED_MANY },
      { "MPOL_WEIGHTED_INTERLEAVE",
        linyaps_box::config::linux_t::memory_policy_t::mode_t::WEIGHTED_INTERLEAVE },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::memory_policy_t::mode_t, std::string_view>>
  memory_policy_mode_to_string{
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::DEFAULT, "MPOL_DEFAULT" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::PREFERRED, "MPOL_PREFERRED" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::BIND, "MPOL_BIND" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::INTERLEAVE, "MPOL_INTERLEAVE" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::LOCAL, "MPOL_LOCAL" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::PREFERRED_MANY,
        "MPOL_PREFERRED_MANY" },
      { linyaps_box::config::linux_t::memory_policy_t::mode_t::WEIGHTED_INTERLEAVE,
        "MPOL_WEIGHTED_INTERLEAVE" },
  };

static const std::vector<std::pair<uint16_t, std::string_view>> memory_policy_flags_to_names{
    { static_cast<uint16_t>(linyaps_box::config::linux_t::memory_policy_t::flag_t::NUMA_BALANCING),
      "MPOL_F_NUMA_BALANCING" },
    { static_cast<uint16_t>(linyaps_box::config::linux_t::memory_policy_t::flag_t::RELATIVE_NODES),
      "MPOL_F_RELATIVE_NODES" },
    { static_cast<uint16_t>(linyaps_box::config::linux_t::memory_policy_t::flag_t::STATIC_NODES),
      "MPOL_F_STATIC_NODES" },
};

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::seccomp_t::arch_t>>
  seccomp_arch_map{
      { "SCMP_ARCH_AARCH64", linyaps_box::config::linux_t::seccomp_t::arch_t::AARCH64 },
      { "SCMP_ARCH_ARM", linyaps_box::config::linux_t::seccomp_t::arch_t::ARM },
      { "SCMP_ARCH_LOONGARCH64", linyaps_box::config::linux_t::seccomp_t::arch_t::LOONGARCH64 },
      { "SCMP_ARCH_M68K", linyaps_box::config::linux_t::seccomp_t::arch_t::M68K },
      { "SCMP_ARCH_MIPS", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS },
      { "SCMP_ARCH_MIPS64", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS64 },
      { "SCMP_ARCH_MIPS64N32", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS64N32 },
      { "SCMP_ARCH_MIPSEL", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL },
      { "SCMP_ARCH_MIPSEL64", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL64 },
      { "SCMP_ARCH_MIPSEL64N32", linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL64N32 },
      { "SCMP_ARCH_PARISC", linyaps_box::config::linux_t::seccomp_t::arch_t::PARISC },
      { "SCMP_ARCH_PARISC64", linyaps_box::config::linux_t::seccomp_t::arch_t::PARISC64 },
      { "SCMP_ARCH_PPC", linyaps_box::config::linux_t::seccomp_t::arch_t::PPC },
      { "SCMP_ARCH_PPC64", linyaps_box::config::linux_t::seccomp_t::arch_t::PPC64 },
      { "SCMP_ARCH_PPC64LE", linyaps_box::config::linux_t::seccomp_t::arch_t::PPC64LE },
      { "SCMP_ARCH_RISCV64", linyaps_box::config::linux_t::seccomp_t::arch_t::RISCV64 },
      { "SCMP_ARCH_S390", linyaps_box::config::linux_t::seccomp_t::arch_t::S390 },
      { "SCMP_ARCH_S390X", linyaps_box::config::linux_t::seccomp_t::arch_t::S390X },
      { "SCMP_ARCH_SH", linyaps_box::config::linux_t::seccomp_t::arch_t::SH },
      { "SCMP_ARCH_SHEB", linyaps_box::config::linux_t::seccomp_t::arch_t::SHEB },
      { "SCMP_ARCH_X32", linyaps_box::config::linux_t::seccomp_t::arch_t::X32 },
      { "SCMP_ARCH_X86", linyaps_box::config::linux_t::seccomp_t::arch_t::X86 },
      { "SCMP_ARCH_X86_64", linyaps_box::config::linux_t::seccomp_t::arch_t::X86_64 },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::seccomp_t::arch_t, std::string_view>>
  seccomp_arch_to_string{
      { linyaps_box::config::linux_t::seccomp_t::arch_t::AARCH64, "SCMP_ARCH_AARCH64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::ARM, "SCMP_ARCH_ARM" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::LOONGARCH64, "SCMP_ARCH_LOONGARCH64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::M68K, "SCMP_ARCH_M68K" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS, "SCMP_ARCH_MIPS" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS64, "SCMP_ARCH_MIPS64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPS64N32, "SCMP_ARCH_MIPS64N32" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL, "SCMP_ARCH_MIPSEL" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL64, "SCMP_ARCH_MIPSEL64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::MIPSEL64N32, "SCMP_ARCH_MIPSEL64N32" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::PARISC, "SCMP_ARCH_PARISC" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::PARISC64, "SCMP_ARCH_PARISC64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::PPC, "SCMP_ARCH_PPC" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::PPC64, "SCMP_ARCH_PPC64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::PPC64LE, "SCMP_ARCH_PPC64LE" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::RISCV64, "SCMP_ARCH_RISCV64" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::S390, "SCMP_ARCH_S390" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::S390X, "SCMP_ARCH_S390X" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::SH, "SCMP_ARCH_SH" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::SHEB, "SCMP_ARCH_SHEB" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::X32, "SCMP_ARCH_X32" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::X86, "SCMP_ARCH_X86" },
      { linyaps_box::config::linux_t::seccomp_t::arch_t::X86_64, "SCMP_ARCH_X86_64" },
  };

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t>>
  seccomp_op_map{
      { "SCMP_CMP_EQ", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::EQ },
      { "SCMP_CMP_GE", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GE },
      { "SCMP_CMP_GT", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GT },
      { "SCMP_CMP_LE", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LE },
      { "SCMP_CMP_LT", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LT },
      { "SCMP_CMP_MASKED_EQ",
        linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::MASKED_EQ },
      { "SCMP_CMP_NE", linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::NE },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t, std::string_view>>
  seccomp_op_to_string{
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::EQ, "SCMP_CMP_EQ" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::NE, "SCMP_CMP_NE" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LT, "SCMP_CMP_LT" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LE, "SCMP_CMP_LE" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GT, "SCMP_CMP_GT" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GE, "SCMP_CMP_GE" },
      { linyaps_box::config::linux_t::seccomp_t::syscall_t::arg_t::op_t::MASKED_EQ,
        "SCMP_CMP_MASKED_EQ" },
  };

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::linux_t::seccomp_t::flag_t>>
  seccomp_flag_map{
      { "SECCOMP_FILTER_FLAG_LOG", linyaps_box::config::linux_t::seccomp_t::flag_t::LOG },
      { "SECCOMP_FILTER_FLAG_SPEC_ALLOW",
        linyaps_box::config::linux_t::seccomp_t::flag_t::SPEC_ALLOW },
      { "SECCOMP_FILTER_FLAG_TSYNC", linyaps_box::config::linux_t::seccomp_t::flag_t::TSYNC },
      { "SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV",
        linyaps_box::config::linux_t::seccomp_t::flag_t::WAIT_KILLABLE_RECV },
  };

static const std::vector<
  std::pair<linyaps_box::config::linux_t::seccomp_t::flag_t, std::string_view>>
  seccomp_flag_to_string{
      { linyaps_box::config::linux_t::seccomp_t::flag_t::LOG, "SECCOMP_FILTER_FLAG_LOG" },
      { linyaps_box::config::linux_t::seccomp_t::flag_t::SPEC_ALLOW,
        "SECCOMP_FILTER_FLAG_SPEC_ALLOW" },
      { linyaps_box::config::linux_t::seccomp_t::flag_t::TSYNC, "SECCOMP_FILTER_FLAG_TSYNC" },
      { linyaps_box::config::linux_t::seccomp_t::flag_t::WAIT_KILLABLE_RECV,
        "SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV" },
  };
#endif

auto parse_range_list(const std::string &s) -> std::vector<unsigned int>
{
    if (s.empty()) {
        return { };
    }
    std::vector<unsigned int> result;
    std::istringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        auto dash = part.find('-');
        if (dash == std::string::npos) {
            result.push_back(static_cast<unsigned int>(std::stoul(part)));
        } else {
            auto start = static_cast<unsigned int>(std::stoul(part.substr(0, dash)));
            auto end = static_cast<unsigned int>(std::stoul(part.substr(dash + 1)));
            auto n = static_cast<size_t>(end - start + 1);
            auto pos = result.size();
            result.resize(pos + n);
            std::iota(result.begin() + static_cast<std::ptrdiff_t>(pos), result.end(), start);
        }
    }
    return result;
}

auto format_range_list(std::vector<unsigned int> v) -> std::string
{
    if (v.empty()) {
        return { };
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());

    std::string result;
    size_t i = 0;
    while (i < v.size()) {
        auto start = v[i];
        auto end = start;
        while (i + 1 < v.size() && v[i + 1] == end + 1) {
            ++i;
            ++end;
        }
        if (!result.empty()) {
            result += ',';
        }
        result += std::to_string(start);
        if (end > start) {
            result += '-';
            result += std::to_string(end);
        }
        ++i;
    }
    return result;
}

} // namespace

namespace linyaps_box {

// --- console_size_t ---

void from_json(const nlohmann::json &j, config::process_t::console_size_t &v)
{
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

void to_json(nlohmann::json &j, const config::process_t::console_size_t &v)
{
    j = nlohmann::json::object({ { "height", v.height }, { "width", v.width } });
}

// --- rlimit_t ---

static const std::vector<
  std::pair<std::string_view, linyaps_box::config::process_t::rlimit_t::type_t>>
  rlimit_type_map{
      { "RLIMIT_AS", linyaps_box::config::process_t::rlimit_t::type_t::AS },
      { "RLIMIT_CORE", linyaps_box::config::process_t::rlimit_t::type_t::CORE },
      { "RLIMIT_CPU", linyaps_box::config::process_t::rlimit_t::type_t::CPU },
      { "RLIMIT_DATA", linyaps_box::config::process_t::rlimit_t::type_t::DATA },
      { "RLIMIT_FSIZE", linyaps_box::config::process_t::rlimit_t::type_t::FSIZE },
      { "RLIMIT_MEMLOCK", linyaps_box::config::process_t::rlimit_t::type_t::MEMLOCK },
      { "RLIMIT_MSGQUEUE", linyaps_box::config::process_t::rlimit_t::type_t::MSGQUEUE },
      { "RLIMIT_NICE", linyaps_box::config::process_t::rlimit_t::type_t::NICE },
      { "RLIMIT_NOFILE", linyaps_box::config::process_t::rlimit_t::type_t::NOFILE },
      { "RLIMIT_NPROC", linyaps_box::config::process_t::rlimit_t::type_t::NPROC },
      { "RLIMIT_RSS", linyaps_box::config::process_t::rlimit_t::type_t::RSS },
      { "RLIMIT_RTPRIO", linyaps_box::config::process_t::rlimit_t::type_t::RTPRIO },
      { "RLIMIT_RTTIME", linyaps_box::config::process_t::rlimit_t::type_t::RTTIME },
      { "RLIMIT_SIGPENDING", linyaps_box::config::process_t::rlimit_t::type_t::SIGPENDING },
      { "RLIMIT_STACK", linyaps_box::config::process_t::rlimit_t::type_t::STACK },
  };

static const std::vector<
  std::pair<linyaps_box::config::process_t::rlimit_t::type_t, std::string_view>>
  rlimit_type_to_string{
      { linyaps_box::config::process_t::rlimit_t::type_t::AS, "RLIMIT_AS" },
      { linyaps_box::config::process_t::rlimit_t::type_t::CORE, "RLIMIT_CORE" },
      { linyaps_box::config::process_t::rlimit_t::type_t::CPU, "RLIMIT_CPU" },
      { linyaps_box::config::process_t::rlimit_t::type_t::DATA, "RLIMIT_DATA" },
      { linyaps_box::config::process_t::rlimit_t::type_t::FSIZE, "RLIMIT_FSIZE" },
      { linyaps_box::config::process_t::rlimit_t::type_t::MEMLOCK, "RLIMIT_MEMLOCK" },
      { linyaps_box::config::process_t::rlimit_t::type_t::MSGQUEUE, "RLIMIT_MSGQUEUE" },
      { linyaps_box::config::process_t::rlimit_t::type_t::NICE, "RLIMIT_NICE" },
      { linyaps_box::config::process_t::rlimit_t::type_t::NOFILE, "RLIMIT_NOFILE" },
      { linyaps_box::config::process_t::rlimit_t::type_t::NPROC, "RLIMIT_NPROC" },
      { linyaps_box::config::process_t::rlimit_t::type_t::RSS, "RLIMIT_RSS" },
      { linyaps_box::config::process_t::rlimit_t::type_t::RTPRIO, "RLIMIT_RTPRIO" },
      { linyaps_box::config::process_t::rlimit_t::type_t::RTTIME, "RLIMIT_RTTIME" },
      { linyaps_box::config::process_t::rlimit_t::type_t::SIGPENDING, "RLIMIT_SIGPENDING" },
      { linyaps_box::config::process_t::rlimit_t::type_t::STACK, "RLIMIT_STACK" },
  };

void from_json(const nlohmann::json &j, config::process_t::rlimit_t &v)
{
    if (!j.contains("type")) {
        throw std::runtime_error("rlimit must contain type");
    }
    auto type_str = j.at("type").get_ref<const std::string &>();
    auto *val = find_in_sorted(rlimit_type_map, type_str);
    if (!val) {
        throw std::runtime_error("unknown rlimit type: " + type_str);
    }
    v.type = *val;
    j.at("soft").get_to(v.soft);
    j.at("hard").get_to(v.hard);
}

void to_json(nlohmann::json &j, const config::process_t::rlimit_t &v)
{
    std::string_view type_str;
    for (const auto &[val, name] : rlimit_type_to_string) {
        if (val == v.type) {
            type_str = name;
            break;
        }
    }
    j = nlohmann::json::object({ { "type", type_str }, { "soft", v.soft }, { "hard", v.hard } });
}

// --- user_t ---

void from_json(const nlohmann::json &j, config::process_t::user_t &v)
{
    j.at("uid").get_to(v.uid);
    j.at("gid").get_to(v.gid);

    if (auto it = j.find("umask"); it != j.end()) {
        v.umask = it->get<mode_t>();
    }

    if (auto it = j.find("additionalGids"); it != j.end()) {
        v.additional_gids = it->get<std::vector<gid_t>>();
    }
}

void to_json(nlohmann::json &j, const config::process_t::user_t &v)
{
    auto obj = nlohmann::json::object({ { "uid", v.uid }, { "gid", v.gid } });
    if (v.umask) {
        obj["umask"] = *v.umask;
    }
    if (v.additional_gids) {
        obj["additionalGids"] = *v.additional_gids;
    }
    j = std::move(obj);
}

// --- capabilities_t ---

#ifdef LINYAPS_BOX_ENABLE_CAP
void from_json(const nlohmann::json &j, config::process_t::capabilities_t &v)
{
    auto parse_cap_set = [&j](const char *name) -> std::vector<cap_value_t> {
        auto it = j.find(name);
        if (it == j.end()) {
            return { };
        }

        std::vector<cap_value_t> result;
        result.reserve(it->size());
        std::transform(it->begin(), it->end(), std::back_inserter(result), [](const auto &elem) {
            cap_value_t val{ 0 };
            auto cap_name = elem.template get_ref<const std::string &>();
            if (cap_from_name(cap_name.c_str(), &val) < 0) {
                throw std::runtime_error("unknown capability: " + cap_name);
            }
            return val;
        });
        return result;
    };

    v.effective = parse_cap_set("effective");
    v.ambient = parse_cap_set("ambient");
    v.bounding = parse_cap_set("bounding");
    v.inheritable = parse_cap_set("inheritable");
    v.permitted = parse_cap_set("permitted");
}

void to_json(nlohmann::json &j, const config::process_t::capabilities_t &v)
{
    auto format_cap_set = [](const std::optional<std::vector<cap_value_t>> &caps)
      -> std::optional<std::vector<std::string>> {
        if (!caps) {
            return std::nullopt;
        }
        std::vector<std::string> result;
        result.reserve(caps->size());
        std::transform(caps->begin(), caps->end(), std::back_inserter(result), [](cap_value_t cap) {
            auto *name = cap_to_name(cap);
            if (name == nullptr) {
                throw std::runtime_error("unknown capability value: "
                                         + std::to_string(static_cast<int>(cap)));
            }
            std::string s(name);
            cap_free(name);
            return s;
        });
        return result;
    };

    j = nlohmann::json::object();
    if (auto effective = format_cap_set(v.effective)) {
        j["effective"] = std::move(*effective);
    }
    if (auto ambient = format_cap_set(v.ambient)) {
        j["ambient"] = std::move(*ambient);
    }
    if (auto bounding = format_cap_set(v.bounding)) {
        j["bounding"] = std::move(*bounding);
    }
    if (auto inheritable = format_cap_set(v.inheritable)) {
        j["inheritable"] = std::move(*inheritable);
    }
    if (auto permitted = format_cap_set(v.permitted)) {
        j["permitted"] = std::move(*permitted);
    }
}
#endif

// --- io_priority_t ---

void from_json(const nlohmann::json &j, config::process_t::io_priority_t &v)
{
    auto class_str = j.at("class").get_ref<const std::string &>();
    auto *val = find_in_sorted(io_priority_class_map, class_str);
    if (!val) {
        throw std::runtime_error("unknown io priority class: " + class_str);
    }
    v.class_ = *val;
    v.priority = j.value("priority", 0);
}

void to_json(nlohmann::json &j, const config::process_t::io_priority_t &v)
{
    std::string_view class_str;
    for (const auto &[val, name] : io_priority_class_to_string) {
        if (val == v.class_) {
            class_str = name;
            break;
        }
    }
    if (class_str.empty()) {
        throw std::runtime_error("unknown io priority class value");
    }

    j = nlohmann::json::object({ { "class", class_str }, { "priority", v.priority } });
}

// --- scheduler_t ---

void from_json(const nlohmann::json &j, config::process_t::scheduler_t &v)
{
    auto policy_str = j.at("policy").get_ref<const std::string &>();
    auto *val = find_in_sorted(scheduler_policy_map, policy_str);
    if (!val) {
        throw std::runtime_error("unknown scheduler policy: " + policy_str);
    }
    v.policy = *val;

    if (auto nice_it = j.find("nice"); nice_it != j.end()) {
        v.nice = nice_it->get<int32_t>();
    }
    if (auto prio_it = j.find("priority"); prio_it != j.end()) {
        v.priority = prio_it->get<int32_t>();
    }

    if (auto flags_it = j.find("flags"); flags_it != j.end()) {
        int flags = 0;
        for (const auto &f : *flags_it) {
            auto flag_str = f.get_ref<const std::string &>();
            for (const auto &[val, name] : scheduler_flags_to_names) {
                if (name == flag_str) {
                    flags |= val;
                    break;
                }
            }
        }
        v.flags = flags;
    }

    if (auto rt_it = j.find("runtime"); rt_it != j.end()) {
        v.runtime = rt_it->get<uint64_t>();
    }
    if (auto dl_it = j.find("deadline"); dl_it != j.end()) {
        v.deadline = dl_it->get<uint64_t>();
    }
    if (auto pr_it = j.find("period"); pr_it != j.end()) {
        v.period = pr_it->get<uint64_t>();
    }
}

void to_json(nlohmann::json &j, const config::process_t::scheduler_t &v)
{
    std::string_view policy_str;
    for (const auto &[val, name] : scheduler_policy_to_string) {
        if (val == v.policy) {
            policy_str = name;
            break;
        }
    }
    if (policy_str.empty()) {
        throw std::runtime_error("unknown scheduler policy value");
    }

    j = nlohmann::json::object({ { "policy", policy_str } });

    if (v.nice) {
        j["nice"] = *v.nice;
    }
    if (v.priority) {
        j["priority"] = *v.priority;
    }
    if (v.flags) {
        nlohmann::json::array_t flag_arr;
        for (const auto &[val, name] : scheduler_flags_to_names) {
            if ((*v.flags & val) == val) {
                flag_arr.push_back(std::string(name));
            }
        }
        j["flags"] = std::move(flag_arr);
    }
    if (v.runtime) {
        j["runtime"] = *v.runtime;
    }
    if (v.deadline) {
        j["deadline"] = *v.deadline;
    }
    if (v.period) {
        j["period"] = *v.period;
    }
}

// --- exec_cpu_affinity_t ---

void from_json(const nlohmann::json &j, config::process_t::exec_cpu_affinity_t &v)
{
    if (auto it = j.find("initial"); it != j.end()) {
        v.initial = it->get<std::string>();
    }
    if (auto it = j.find("final"); it != j.end()) {
        v.final = it->get<std::string>();
    }
}

void to_json(nlohmann::json &j, const config::process_t::exec_cpu_affinity_t &v)
{
    j = nlohmann::json::object();
    if (v.initial) {
        j["initial"] = *v.initial;
    }
    if (v.final) {
        j["final"] = *v.final;
    }
}

// --- process_t ---

void from_json(const nlohmann::json &j, config::process_t &v)
{
    if (auto it = j.find("terminal"); it != j.end()) {
        it->get_to(v.terminal);
    }

    if (v.terminal) {
        if (auto it = j.find("consoleSize"); it != j.end()) {
            v.console_size = it->get<config::process_t::console_size_t>();
        }
    }

    j.at("cwd").get_to(v.cwd);
    if (!v.cwd.is_absolute()) {
        throw std::runtime_error("process.cwd must be an absolute path, got: " + v.cwd.string());
    }

    if (auto it = j.find("env"); it != j.end()) {
        it->get_to(v.env);
        for (const auto &e : *v.env) {
            if (e.find('=') == std::string::npos) {
                throw std::runtime_error("invalid env entry: " + e);
            }
        }
    }

    j.at("args").get_to(v.args);
    if (v.args.empty()) {
        throw std::runtime_error("process.args must not be empty");
    }

    if (auto it = j.find("rlimits"); it != j.end()) {
        v.rlimits = it->get<std::vector<config::process_t::rlimit_t>>();
        std::vector<config::process_t::rlimit_t::type_t> seen_rl;
        for (const auto &rl : *v.rlimits) {
            if (std::find(seen_rl.begin(), seen_rl.end(), rl.type) != seen_rl.end()) {
                throw std::runtime_error(std::string(to_string_view(rl.type))
                                         + ": duplicate rlimit type");
            }
            seen_rl.push_back(rl.type);
        }
    }

    if (auto it = j.find("apparmorProfile"); it != j.end()) {
        v.apparmor_profile = it->get<std::string>();
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (auto it = j.find("capabilities"); it != j.end()) {
        v.capabilities = it->get<config::process_t::capabilities_t>();
    }
#endif

    if (auto it = j.find("noNewPrivileges"); it != j.end()) {
        it->get_to(v.no_new_privileges);
    }

    if (auto it = j.find("oomScoreAdj"); it != j.end()) {
        v.oom_score_adj = it->get<int>();
    }

    if (auto it = j.find("scheduler"); it != j.end()) {
        v.scheduler = it->get<config::process_t::scheduler_t>();
    }

    if (auto it = j.find("selinuxLabel"); it != j.end()) {
        v.selinux_label = it->get<std::string>();
    }

    if (auto it = j.find("ioPriority"); it != j.end()) {
        v.io_priority = it->get<config::process_t::io_priority_t>();
    }

    if (auto it = j.find("execCPUAffinity"); it != j.end()) {
        v.exec_cpu_affinity = it->get<config::process_t::exec_cpu_affinity_t>();
    }

    j.at("user").get_to(v.user);
}

void to_json(nlohmann::json &j, const config::process_t &v)
{
    j = nlohmann::json::object();

    if (v.terminal) {
        j["terminal"] = true;
        if (v.console_size) {
            j["consoleSize"] = *v.console_size;
        }
    }

    j["cwd"] = v.cwd.string();
    j["args"] = v.args;

    if (v.env) {
        j["env"] = *v.env;
    }
    if (v.rlimits) {
        j["rlimits"] = *v.rlimits;
    }
    if (v.apparmor_profile) {
        j["apparmorProfile"] = *v.apparmor_profile;
    }
    if (v.no_new_privileges) {
        j["noNewPrivileges"] = *v.no_new_privileges;
    }
    if (v.oom_score_adj) {
        j["oomScoreAdj"] = *v.oom_score_adj;
    }
    if (v.selinux_label) {
        j["selinuxLabel"] = *v.selinux_label;
    }
    if (v.scheduler) {
        j["scheduler"] = *v.scheduler;
    }
    if (v.io_priority) {
        j["ioPriority"] = *v.io_priority;
    }
    if (v.exec_cpu_affinity) {
        j["execCPUAffinity"] = *v.exec_cpu_affinity;
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (v.capabilities) {
        j["capabilities"] = *v.capabilities;
    }
#endif

    j["user"] = v.user;
}

// --- id_mapping_t ---

void from_json(const nlohmann::json &j, config::linux_t::id_mapping_t &v)
{
    j.at("hostID").get_to(v.host_id);
    j.at("containerID").get_to(v.container_id);
    j.at("size").get_to(v.size);
}

void to_json(nlohmann::json &j, const config::linux_t::id_mapping_t &v)
{
    j = nlohmann::json::object(
      { { "containerID", v.container_id }, { "hostID", v.host_id }, { "size", v.size } });
}

// --- namespace_t ---

void from_json(const nlohmann::json &j, config::linux_t::namespace_t &v)
{
    auto type_str = j.at("type").get_ref<const std::string &>();
    const auto *val = find_in_sorted(namespace_type_map, type_str);
    if (val == nullptr) {
        throw std::runtime_error("unsupported namespace type: " + type_str);
    }
    v.type_ = *val;

    if (auto path_it = j.find("path"); path_it != j.end()) {
        auto path = path_it->get<std::string>();
        if (path.empty()) {
            throw std::runtime_error("namespace path must not be empty for type: " + type_str);
        }

        auto fs_path = std::filesystem::path{ path };
        if (!fs_path.is_absolute()) {
            throw std::runtime_error("namespace path must be absolute for type: " + type_str
                                     + ", got: " + path);
        }

        v.path = std::move(fs_path);
    }
}

void to_json(nlohmann::json &j, const config::linux_t::namespace_t &v)
{
    std::string_view type_str;
    for (const auto &[val, name] : namespace_type_to_string) {
        if (val == v.type_) {
            type_str = name;
            break;
        }
    }
    if (type_str.empty()) {
        throw std::runtime_error("unsupported namespace type value");
    }

    j = nlohmann::json::object({ { "type", type_str } });
    if (v.path) {
        j["path"] = v.path->string();
    }
}

// --- time_offset_t ---

void from_json(const nlohmann::json &j, config::linux_t::time_offset_t &v)
{
    j.at("secs").get_to(v.secs);
    j.at("nanosecs").get_to(v.nanosecs);
}

void to_json(nlohmann::json &j, const config::linux_t::time_offset_t &v)
{
    j = nlohmann::json::object({ { "secs", v.secs }, { "nanosecs", v.nanosecs } });
}

// --- device_t ---

void from_json(const nlohmann::json &j, config::linux_t::device_t &v)
{
    j.at("type").get_to(v.type);
    j.at("path").get_to(v.path);

    if (auto it = j.find("fileMode"); it != j.end()) {
        v.mode = it->get<uint32_t>();
    }
    if (auto it = j.find("major"); it != j.end()) {
        v.major = it->get<uint32_t>();
    }
    if (auto it = j.find("minor"); it != j.end()) {
        v.minor = it->get<uint32_t>();
    }
    if (auto it = j.find("uid"); it != j.end()) {
        v.uid = it->get<uint32_t>();
    }
    if (auto it = j.find("gid"); it != j.end()) {
        v.gid = it->get<uint32_t>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::device_t &v)
{
    j = nlohmann::json::object({ { "type", v.type }, { "path", v.path.string() } });
    if (v.mode) {
        j["fileMode"] = *v.mode;
    }
    if (v.major) {
        j["major"] = *v.major;
    }
    if (v.minor) {
        j["minor"] = *v.minor;
    }
    if (v.uid) {
        j["uid"] = *v.uid;
    }
    if (v.gid) {
        j["gid"] = *v.gid;
    }
}

// --- network_device_t ---

void from_json(const nlohmann::json &j, config::linux_t::network_device_t &v)
{
    if (auto it = j.find("name"); it != j.end()) {
        v.name = it->get<std::string>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::network_device_t &v)
{
    j = nlohmann::json::object();
    if (v.name) {
        j["name"] = *v.name;
    }
}

// --- allowed_device_t ---

void from_json(const nlohmann::json &j, config::linux_t::allowed_device_t &v)
{
    j.at("allow").get_to(v.allow);
    if (auto it = j.find("type"); it != j.end()) {
        v.type = it->get<std::string>();
    }
    if (auto it = j.find("major"); it != j.end()) {
        v.major = it->get<int64_t>();
    }
    if (auto it = j.find("minor"); it != j.end()) {
        v.minor = it->get<int64_t>();
    }
    if (auto it = j.find("access"); it != j.end()) {
        v.access = it->get<std::string>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::allowed_device_t &v)
{
    j = nlohmann::json::object({ { "allow", v.allow } });
    if (v.type) {
        j["type"] = *v.type;
    }
    if (v.major) {
        j["major"] = *v.major;
    }
    if (v.minor) {
        j["minor"] = *v.minor;
    }
    if (v.access) {
        j["access"] = *v.access;
    }
}

// --- memory_t ---

void from_json(const nlohmann::json &j, config::linux_t::memory_t &v)
{
    if (auto it = j.find("limit"); it != j.end()) {
        v.limit = it->get<int64_t>();
    }
    if (auto it = j.find("reservation"); it != j.end()) {
        v.reservation = it->get<int64_t>();
    }
    if (auto it = j.find("swap"); it != j.end()) {
        v.swap = it->get<int64_t>();
    }
    if (auto it = j.find("kernel"); it != j.end()) {
        v.kernel = it->get<int64_t>();
    }
    if (auto it = j.find("kernelTCP"); it != j.end()) {
        v.kernel_tcp = it->get<int64_t>();
    }
    if (auto it = j.find("swappiness"); it != j.end()) {
        v.swappiness = it->get<uint64_t>();
    }
    if (auto it = j.find("disableOOMKiller"); it != j.end()) {
        v.disable_OOM_killer = it->get<bool>();
    }
    if (auto it = j.find("useHierarchy"); it != j.end()) {
        v.use_hierarchy = it->get<bool>();
    }
    if (auto it = j.find("checkBeforeUpdate"); it != j.end()) {
        v.check_before_update = it->get<bool>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::memory_t &v)
{
    j = nlohmann::json::object();
    if (v.limit) {
        j["limit"] = *v.limit;
    }
    if (v.reservation) {
        j["reservation"] = *v.reservation;
    }
    if (v.swap) {
        j["swap"] = *v.swap;
    }
    if (v.kernel) {
        j["kernel"] = *v.kernel;
    }
    if (v.kernel_tcp) {
        j["kernelTCP"] = *v.kernel_tcp;
    }
    if (v.swappiness) {
        j["swappiness"] = *v.swappiness;
    }
    if (v.disable_OOM_killer) {
        j["disableOOMKiller"] = *v.disable_OOM_killer;
    }
    if (v.use_hierarchy) {
        j["useHierarchy"] = *v.use_hierarchy;
    }
    if (v.check_before_update) {
        j["checkBeforeUpdate"] = *v.check_before_update;
    }
}

// --- cpu_t ---

void from_json(const nlohmann::json &j, config::linux_t::cpu_t &v)
{
    if (auto it = j.find("shares"); it != j.end()) {
        v.shares = it->get<uint64_t>();
    }
    if (auto it = j.find("quota"); it != j.end()) {
        v.quota = it->get<int64_t>();
    }
    if (auto it = j.find("burst"); it != j.end()) {
        v.burst = it->get<uint64_t>();
    }
    if (auto it = j.find("period"); it != j.end()) {
        v.period = it->get<uint64_t>();
    }
    if (auto it = j.find("realtimeRuntime"); it != j.end()) {
        v.realtime_runtime = it->get<int64_t>();
    }
    if (auto it = j.find("realtimePeriod"); it != j.end()) {
        v.realtime_period = it->get<uint64_t>();
    }
    if (auto it = j.find("cpus"); it != j.end()) {
        v.cpus = parse_range_list(it->get_ref<const std::string &>());
    }
    if (auto it = j.find("mems"); it != j.end()) {
        v.mems = it->get<std::string>();
    }
    if (auto it = j.find("idle"); it != j.end()) {
        auto val = it->get<int64_t>();
        v.idle =
          (val == 1) ? config::linux_t::cpu_t::idle_t::IDLE : config::linux_t::cpu_t::idle_t::NONE;
    }
}

void to_json(nlohmann::json &j, const config::linux_t::cpu_t &v)
{
    j = nlohmann::json::object();
    if (v.shares) {
        j["shares"] = *v.shares;
    }
    if (v.quota) {
        j["quota"] = *v.quota;
    }
    if (v.burst) {
        j["burst"] = *v.burst;
    }
    if (v.period) {
        j["period"] = *v.period;
    }
    if (v.realtime_runtime) {
        j["realtimeRuntime"] = *v.realtime_runtime;
    }
    if (v.realtime_period) {
        j["realtimePeriod"] = *v.realtime_period;
    }
    if (v.cpus) {
        j["cpus"] = format_range_list(*v.cpus);
    }
    if (v.mems) {
        j["mems"] = *v.mems;
    }
    if (v.idle) {
        j["idle"] = (*v.idle == config::linux_t::cpu_t::idle_t::IDLE) ? 1 : 0;
    }
}

// --- block_io_t ---

void from_json(const nlohmann::json &j, config::linux_t::block_io_t::weight_device_t &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);
    if (auto it = j.find("weight"); it != j.end()) {
        v.weight = it->get<uint16_t>();
    }
    if (auto it = j.find("leafWeight"); it != j.end()) {
        v.leaf_weight = it->get<uint16_t>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::block_io_t::weight_device_t &v)
{
    j = nlohmann::json::object({ { "major", v.major }, { "minor", v.minor } });
    if (v.weight) {
        j["weight"] = *v.weight;
    }
    if (v.leaf_weight) {
        j["leafWeight"] = *v.leaf_weight;
    }
}

void from_json(const nlohmann::json &j, config::linux_t::block_io_t::throttle_device_t &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);
    j.at("rate").get_to(v.rate);
}

void to_json(nlohmann::json &j, const config::linux_t::block_io_t::throttle_device_t &v)
{
    j = nlohmann::json::object({ { "major", v.major }, { "minor", v.minor }, { "rate", v.rate } });
}

void from_json(const nlohmann::json &j, config::linux_t::block_io_t &v)
{
    if (auto it = j.find("weight"); it != j.end()) {
        v.weight = it->get<uint16_t>();
    }
    if (auto it = j.find("leafWeight"); it != j.end()) {
        v.leaf_weight = it->get<uint16_t>();
    }
    if (auto it = j.find("weightDevice"); it != j.end()) {
        v.weight_devices = it->get<std::vector<config::linux_t::block_io_t::weight_device_t>>();
    }
    if (auto it = j.find("throttleReadBpsDevice"); it != j.end()) {
        v.throttle_read_bps_device =
          it->get<std::vector<config::linux_t::block_io_t::throttle_device_t>>();
    }
    if (auto it = j.find("throttleWriteBpsDevice"); it != j.end()) {
        v.throttle_write_bps_device =
          it->get<std::vector<config::linux_t::block_io_t::throttle_device_t>>();
    }
    if (auto it = j.find("throttleReadIOPSDevice"); it != j.end()) {
        v.throttle_read_iops_device =
          it->get<std::vector<config::linux_t::block_io_t::throttle_device_t>>();
    }
    if (auto it = j.find("throttleWriteIOPSDevice"); it != j.end()) {
        v.throttle_write_iops_device =
          it->get<std::vector<config::linux_t::block_io_t::throttle_device_t>>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::block_io_t &v)
{
    j = nlohmann::json::object();
    if (v.weight) {
        j["weight"] = *v.weight;
    }
    if (v.leaf_weight) {
        j["leafWeight"] = *v.leaf_weight;
    }
    if (v.weight_devices) {
        j["weightDevice"] = *v.weight_devices;
    }
    if (v.throttle_read_bps_device) {
        j["throttleReadBpsDevice"] = *v.throttle_read_bps_device;
    }
    if (v.throttle_write_bps_device) {
        j["throttleWriteBpsDevice"] = *v.throttle_write_bps_device;
    }
    if (v.throttle_read_iops_device) {
        j["throttleReadIOPSDevice"] = *v.throttle_read_iops_device;
    }
    if (v.throttle_write_iops_device) {
        j["throttleWriteIOPSDevice"] = *v.throttle_write_iops_device;
    }
}

// --- hugepage_limit_t ---

void from_json(const nlohmann::json &j, config::linux_t::hugepage_limit_t &v)
{
    j.at("pageSize").get_to(v.page_size);
    j.at("limit").get_to(v.limit);
}

void to_json(nlohmann::json &j, const config::linux_t::hugepage_limit_t &v)
{
    j = nlohmann::json::object({ { "pageSize", v.page_size }, { "limit", v.limit } });
}

// --- network_t::priority_t ---

void from_json(const nlohmann::json &j, config::linux_t::network_t::priority_t &v)
{
    j.at("name").get_to(v.name);
    j.at("priority").get_to(v.priority);
}

void to_json(nlohmann::json &j, const config::linux_t::network_t::priority_t &v)
{
    j = nlohmann::json::object({ { "name", v.name }, { "priority", v.priority } });
}

// --- network_t ---

void from_json(const nlohmann::json &j, config::linux_t::network_t &v)
{
    if (auto it = j.find("classID"); it != j.end()) {
        v.class_id = it->get<uint32_t>();
    }
    if (auto it = j.find("priorities"); it != j.end()) {
        v.priorities = it->get<std::vector<config::linux_t::network_t::priority_t>>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::network_t &v)
{
    j = nlohmann::json::object();
    if (v.class_id) {
        j["classID"] = *v.class_id;
    }
    if (v.priorities) {
        j["priorities"] = *v.priorities;
    }
}

// --- pids_t ---

void from_json(const nlohmann::json &j, config::linux_t::pids_t &v)
{
    if (auto it = j.find("limit"); it != j.end()) {
        v.limit = it->get<int64_t>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::pids_t &v)
{
    j = nlohmann::json::object();
    if (v.limit) {
        j["limit"] = *v.limit;
    }
}

// --- rdma_t ---

void from_json(const nlohmann::json &j, config::linux_t::rdma_t &v)
{
    if (auto it = j.find("hcaHandles"); it != j.end()) {
        v.hca_handles = it->get<uint32_t>();
    }
    if (auto it = j.find("hcaObjects"); it != j.end()) {
        v.hca_objects = it->get<uint32_t>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::rdma_t &v)
{
    j = nlohmann::json::object();
    if (v.hca_handles) {
        j["hcaHandles"] = *v.hca_handles;
    }
    if (v.hca_objects) {
        j["hcaObjects"] = *v.hca_objects;
    }
}

// --- intel_rdt_t ---

void from_json(const nlohmann::json &j, config::linux_t::intel_rdt_t &v)
{
    if (auto it = j.find("closID"); it != j.end()) {
        v.clos_id = it->get<std::string>();
    }
    if (auto it = j.find("l3CacheSchema"); it != j.end()) {
        v.l3_cache_schema = it->get<std::string>();
    }
    if (auto it = j.find("memBwSchema"); it != j.end()) {
        v.memory_bandwidth_schema = it->get<std::string>();
    }
    if (auto it = j.find("schemata"); it != j.end()) {
        v.schemata = it->get<std::vector<std::string>>();
    }
    if (auto it = j.find("enableMonitoring"); it != j.end()) {
        v.enable_monitoring = it->get<bool>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::intel_rdt_t &v)
{
    j = nlohmann::json::object();
    if (v.clos_id) {
        j["closID"] = *v.clos_id;
    }
    if (v.l3_cache_schema) {
        j["l3CacheSchema"] = *v.l3_cache_schema;
    }
    if (v.memory_bandwidth_schema) {
        j["memBwSchema"] = *v.memory_bandwidth_schema;
    }
    if (v.schemata) {
        j["schemata"] = *v.schemata;
    }
    if (v.enable_monitoring) {
        j["enableMonitoring"] = *v.enable_monitoring;
    }
}

// --- memory_policy_t ---

void from_json(const nlohmann::json &j, config::linux_t::memory_policy_t &v)
{
    auto mode_str = j.at("mode").get_ref<const std::string &>();
    auto *val = find_in_sorted(memory_policy_mode_map, mode_str);
    if (!val) {
        throw std::runtime_error("unknown memory policy mode: " + mode_str);
    }
    v.mode = *val;

    if (auto nodes_it = j.find("nodes"); nodes_it != j.end()) {
        auto nodes_str = nodes_it->get_ref<const std::string &>();
        auto parsed = parse_range_list(nodes_str);
        v.nodes = std::vector<int>(parsed.begin(), parsed.end());
    }

    if (auto flags_it = j.find("flags"); flags_it != j.end()) {
        uint16_t combined = 0;
        for (const auto &f : *flags_it) {
            auto flag_str = f.get_ref<const std::string &>();
            for (const auto &[val, name] : memory_policy_flags_to_names) {
                if (name == flag_str) {
                    combined |= val;
                    break;
                }
            }
        }
        v.flags = static_cast<config::linux_t::memory_policy_t::flag_t>(combined);
    }
}

void to_json(nlohmann::json &j, const config::linux_t::memory_policy_t &v)
{
    std::string_view mode_str;
    for (const auto &[val, name] : memory_policy_mode_to_string) {
        if (val == v.mode) {
            mode_str = name;
            break;
        }
    }
    if (mode_str.empty()) {
        throw std::runtime_error("unknown memory policy mode value");
    }

    j = nlohmann::json::object({ { "mode", mode_str } });

    if (v.nodes) {
        std::vector<unsigned int> unsigned_nodes(v.nodes->begin(), v.nodes->end());
        j["nodes"] = format_range_list(unsigned_nodes);
    }
    if (v.flags) {
        auto raw = static_cast<uint16_t>(*v.flags);
        nlohmann::json::array_t flag_arr;
        for (const auto &[val, name] : memory_policy_flags_to_names) {
            if ((raw & val) == val) {
                flag_arr.push_back(std::string(name));
            }
        }
        j["flags"] = std::move(flag_arr);
    }
}

// --- personality_t ---

void from_json(const nlohmann::json &j, config::linux_t::personality_t &v)
{
    auto domain_str = j.at("domain").get_ref<const std::string &>();
    auto *val = find_in_sorted(personality_domain_map, domain_str);
    if (!val) {
        throw std::runtime_error("unknown personality domain: " + domain_str);
    }
    v.domain = *val;

    if (auto flags_it = j.find("flags"); flags_it != j.end()) {
        v.flags = flags_it->get<std::vector<std::string>>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::personality_t &v)
{
    std::string_view domain_str;
    for (const auto &[val, name] : personality_domain_to_string) {
        if (val == v.domain) {
            domain_str = name;
            break;
        }
    }
    if (domain_str.empty()) {
        throw std::runtime_error("unknown personality domain value");
    }

    j = nlohmann::json::object({ { "domain", domain_str } });
    if (v.flags) {
        j["flags"] = *v.flags;
    }
}

// --- seccomp_t ---

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
void from_json(const nlohmann::json &j, config::linux_t::seccomp_t::syscall_t::arg_t &v)
{
    j.at("index").get_to(v.index);
    j.at("value").get_to(v.value);
    v.value_two = j.value("valueTwo", uint64_t{ 0 });

    auto op_str = j.at("op").get_ref<const std::string &>();
    auto *val = find_in_sorted(seccomp_op_map, op_str);
    if (!val) {
        throw std::runtime_error("unknown seccomp op: " + op_str);
    }
    v.op = *val;
}

void to_json(nlohmann::json &j, const config::linux_t::seccomp_t::syscall_t::arg_t &v)
{
    std::string_view op_str;
    for (const auto &[val, name] : seccomp_op_to_string) {
        if (val == v.op) {
            op_str = name;
            break;
        }
    }
    if (op_str.empty()) {
        throw std::runtime_error("unknown seccomp op value");
    }

    j = nlohmann::json::object({ { "index", v.index }, { "value", v.value }, { "op", op_str } });
    if (v.value_two != 0) {
        j["valueTwo"] = v.value_two;
    }
}

void from_json(const nlohmann::json &j, config::linux_t::seccomp_t::syscall_t &v)
{
    j.at("names").get_to(v.name);
    j.at("action").get_to(v.action);

    if (auto it = j.find("errnoRet"); it != j.end()) {
        v.errno_ret = it->get<uint>();
    }
    if (auto it = j.find("args"); it != j.end()) {
        v.args = it->get<std::vector<config::linux_t::seccomp_t::syscall_t::arg_t>>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::seccomp_t::syscall_t &v)
{
    j = nlohmann::json::object({ { "names", v.name }, { "action", v.action } });
    if (v.errno_ret) {
        j["errnoRet"] = *v.errno_ret;
    }
    if (v.args) {
        j["args"] = *v.args;
    }
}

void from_json(const nlohmann::json &j, config::linux_t::seccomp_t &v)
{
    j.at("defaultAction").get_to(v.default_action);

    if (auto it = j.find("defaultErrnoRet"); it != j.end()) {
        v.default_errno_ret = it->get<uint>();
    }
    if (auto it = j.find("architectures"); it != j.end()) {
        std::vector<config::linux_t::seccomp_t::arch_t> archs;
        archs.reserve(it->size());
        for (const auto &elem : *it) {
            auto arch_str = elem.get_ref<const std::string &>();
            auto *arch_val = find_in_sorted(seccomp_arch_map, arch_str);
            if (!arch_val) {
                throw std::runtime_error("unknown seccomp arch: " + arch_str);
            }
            archs.push_back(*arch_val);
        }
        v.architectures = std::move(archs);
    }
    if (auto it = j.find("flags"); it != j.end()) {
        std::underlying_type_t<config::linux_t::seccomp_t::flag_t> combined = 0;
        for (const auto &elem : *it) {
            auto flag_str = elem.get_ref<const std::string &>();
            auto *flag_val = find_in_sorted(seccomp_flag_map, flag_str);
            if (flag_val) {
                combined |= static_cast<std::underlying_type_t<config::linux_t::seccomp_t::flag_t>>(
                  *flag_val);
            }
        }
        v.flags = static_cast<config::linux_t::seccomp_t::flag_t>(combined);
    }
    if (auto it = j.find("listenerPath"); it != j.end()) {
        v.listener_path = it->get<std::filesystem::path>();
    }
    if (auto it = j.find("listenerMetadata"); it != j.end()) {
        v.listener_metadata = it->get<std::string>();
    }
    if (auto it = j.find("syscalls"); it != j.end()) {
        v.syscalls = it->get<std::vector<config::linux_t::seccomp_t::syscall_t>>();
    }
}

void to_json(nlohmann::json &j, const config::linux_t::seccomp_t &v)
{
    j = nlohmann::json::object({ { "defaultAction", v.default_action } });

    if (v.default_errno_ret) {
        j["defaultErrnoRet"] = *v.default_errno_ret;
    }
    if (v.architectures) {
        nlohmann::json::array_t arch_arr;
        arch_arr.reserve(v.architectures->size());
        for (auto arch : *v.architectures) {
            std::string_view arch_str;
            for (const auto &[av, an] : seccomp_arch_to_string) {
                if (av == arch) {
                    arch_str = an;
                    break;
                }
            }
            if (arch_str.empty()) {
                throw std::runtime_error("unknown seccomp arch value");
            }
            arch_arr.push_back(std::string(arch_str));
        }
        j["architectures"] = std::move(arch_arr);
    }
    if (v.flags) {
        nlohmann::json::array_t flag_arr;
        auto raw =
          static_cast<std::underlying_type_t<config::linux_t::seccomp_t::flag_t>>(*v.flags);
        for (const auto &[fv, fname] : seccomp_flag_to_string) {
            if (raw & static_cast<std::underlying_type_t<config::linux_t::seccomp_t::flag_t>>(fv)) {
                flag_arr.push_back(std::string(fname));
            }
        }
        j["flags"] = std::move(flag_arr);
    }
    if (v.listener_path) {
        j["listenerPath"] = v.listener_path->string();
    }
    if (v.listener_metadata) {
        j["listenerMetadata"] = *v.listener_metadata;
    }
    if (v.syscalls) {
        j["syscalls"] = *v.syscalls;
    }
}
#endif
// --- linux_t ---

void from_json(const nlohmann::json &j, config::linux_t &v)
{
    if (auto it = j.find("uidMappings"); it != j.end()) {
        v.uid_mappings = it->get<std::vector<config::linux_t::id_mapping_t>>();
    }

    if (auto it = j.find("gidMappings"); it != j.end()) {
        v.gid_mappings = it->get<std::vector<config::linux_t::id_mapping_t>>();
    }

    if (auto it = j.find("namespaces"); it != j.end()) {
        v.namespaces = it->get<std::vector<config::linux_t::namespace_t>>();

        auto seen{ config::linux_t::namespace_t::type::NONE };
        for (const auto &ns : *v.namespaces) {
            if ((seen & ns.type_) != config::linux_t::namespace_t::type::NONE) {
                throw std::runtime_error(std::string(to_string_view(ns.type_))
                                         + ": duplicate namespace type");
            }

            seen = seen | ns.type_;
        }
    }

    if (auto it = j.find("devices"); it != j.end()) {
        v.devices = it->get<std::vector<config::linux_t::device_t>>();
    }

    if (auto it = j.find("netDevices"); it != j.end()) {
        v.network_devices = it->get<std::vector<config::linux_t::network_device_t>>();
    }

    if (auto it = j.find("cgroupsPath"); it != j.end()) {
        v.cgroups_path = it->get<std::string>();
    }

    if (auto it = j.find("maskedPaths"); it != j.end()) {
        v.masked_paths = it->get<std::vector<std::filesystem::path>>();
    }

    if (auto it = j.find("readonlyPaths"); it != j.end()) {
        v.readonly_paths = it->get<std::vector<std::filesystem::path>>();
    }

    if (auto it = j.find("mountLabel"); it != j.end()) {
        v.mount_label = it->get<std::string>();
    }

    if (auto it = j.find("rootfsPropagation"); it != j.end()) {
        auto val = it->get_ref<const std::string &>();
        auto *prop_val = find_in_sorted(rootfs_propagation_map, val);
        if (!prop_val) {
            throw std::runtime_error("unsupported rootfs propagation: " + val);
        }
        v.rootfs_propagation = static_cast<config::linux_t::rootfs_propagation_t>(*prop_val);
    }

    if (auto it = j.find("sysctl"); it != j.end()) {
        v.sysctl = it->get<std::unordered_map<std::string, std::string>>();
    }

    if (auto it = j.find("timeOffsets"); it != j.end()) {
        std::unordered_map<std::string, config::linux_t::time_offset_t> offsets;
        for (auto &[clock_name, offset_json] : it->items()) {
            offsets[clock_name] = offset_json.get<config::linux_t::time_offset_t>();
        }
        v.time_offsets = std::move(offsets);
    }

    if (auto it = j.find("personality"); it != j.end()) {
        v.personality = it->get<config::linux_t::personality_t>();
    }

    if (auto it = j.find("memoryPolicy"); it != j.end()) {
        v.memory_policy = it->get<config::linux_t::memory_policy_t>();
    }

    if (auto it = j.find("intelRdt"); it != j.end()) {
        v.intel_rdt = it->get<config::linux_t::intel_rdt_t>();
    }

    // Resources sub-object
    if (auto res_it = j.find("resources"); res_it != j.end()) {
        const auto &r = *res_it;

        if (auto it = r.find("unified"); it != r.end()) {
            v.unified = it->get<std::unordered_map<std::string, std::string>>();
        }
        if (auto it = r.find("devices"); it != r.end()) {
            v.allowed_devices = it->get<std::vector<config::linux_t::allowed_device_t>>();
        }
        if (auto it = r.find("pids"); it != r.end()) {
            v.pids = it->get<config::linux_t::pids_t>();
        }
        if (auto it = r.find("memory"); it != r.end()) {
            v.memory = it->get<config::linux_t::memory_t>();
        }
        if (auto it = r.find("cpu"); it != r.end()) {
            v.cpu = it->get<config::linux_t::cpu_t>();
        }
        if (auto it = r.find("hugepageLimits"); it != r.end()) {
            v.hugepage_limits = it->get<std::vector<config::linux_t::hugepage_limit_t>>();
        }
        if (auto it = r.find("blockIO"); it != r.end()) {
            v.block_io = it->get<config::linux_t::block_io_t>();
        }
        if (auto it = r.find("network"); it != r.end()) {
            v.network = it->get<config::linux_t::network_t>();
        }
        if (auto it = r.find("rdma"); it != r.end()) {
            v.rdma = it->get<std::unordered_map<std::string, config::linux_t::rdma_t>>();
        }
    }

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
    if (auto it = j.find("seccomp"); it != j.end()) {
        v.seccomp = it->get<config::linux_t::seccomp_t>();
    }
#endif
}

void to_json(nlohmann::json &j, const config::linux_t &v)
{
    j = nlohmann::json::object();

    if (v.uid_mappings) {
        j["uidMappings"] = *v.uid_mappings;
    }
    if (v.gid_mappings) {
        j["gidMappings"] = *v.gid_mappings;
    }
    if (v.namespaces) {
        j["namespaces"] = *v.namespaces;
    }
    if (v.devices) {
        j["devices"] = *v.devices;
    }
    if (v.network_devices) {
        j["netDevices"] = *v.network_devices;
    }
    if (v.cgroups_path) {
        j["cgroupsPath"] = *v.cgroups_path;
    }
    if (v.masked_paths) {
        j["maskedPaths"] = *v.masked_paths;
    }
    if (v.readonly_paths) {
        j["readonlyPaths"] = *v.readonly_paths;
    }
    if (v.mount_label) {
        j["mountLabel"] = *v.mount_label;
    }
    if (v.rootfs_propagation) {
        auto raw = static_cast<unsigned int>(*v.rootfs_propagation);
        for (const auto &[val, name] : rootfs_propagation_to_string) {
            if (val == raw) {
                j["rootfsPropagation"] = name;
                break;
            }
        }
    }
    if (v.sysctl) {
        j["sysctl"] = *v.sysctl;
    }
    if (v.time_offsets) {
        nlohmann::json offsets_obj = nlohmann::json::object();
        for (const auto &[clock, offset] : *v.time_offsets) {
            offsets_obj[clock] = offset;
        }
        j["timeOffsets"] = std::move(offsets_obj);
    }
    if (v.personality) {
        j["personality"] = *v.personality;
    }
    if (v.memory_policy) {
        j["memoryPolicy"] = *v.memory_policy;
    }
    if (v.intel_rdt) {
        j["intelRdt"] = *v.intel_rdt;
    }
#ifdef LINYAPS_BOX_ENABLE_SECCOMP
    if (v.seccomp) {
        j["seccomp"] = *v.seccomp;
    }
#endif

    // Resources sub-object
    bool has_resources = v.unified || v.allowed_devices || v.pids || v.memory || v.cpu
      || v.hugepage_limits || v.block_io || v.network || v.rdma;
    if (has_resources) {
        nlohmann::json res = nlohmann::json::object();
        if (v.unified) {
            res["unified"] = *v.unified;
        }
        if (v.allowed_devices) {
            res["devices"] = *v.allowed_devices;
        }
        if (v.pids) {
            res["pids"] = *v.pids;
        }
        if (v.memory) {
            res["memory"] = *v.memory;
        }
        if (v.cpu) {
            res["cpu"] = *v.cpu;
        }
        if (v.hugepage_limits) {
            res["hugepageLimits"] = *v.hugepage_limits;
        }
        if (v.block_io) {
            res["blockIO"] = *v.block_io;
        }
        if (v.network) {
            res["network"] = *v.network;
        }
        if (v.rdma) {
            res["rdma"] = *v.rdma;
        }
        j["resources"] = std::move(res);
    }
}

// --- hook_t ---

void from_json(const nlohmann::json &j, config::hooks_t::hook_t &v)
{
    j.at("path").get_to(v.path);
    if (!v.path.is_absolute()) {
        throw std::runtime_error("hook path must be absolute");
    }

    if (auto it = j.find("args"); it != j.end()) {
        v.args = it->get<std::vector<std::string>>();
    }

    if (auto it = j.find("env"); it != j.end()) {
        std::unordered_map<std::string, std::string> env;
        for (auto &e : it->get<std::vector<std::string>>()) {
            auto pos = e.find('=');
            if (pos == std::string::npos) {
                throw std::runtime_error("invalid env entry: " + e);
            }
            auto key = e.substr(0, pos);
            e.erase(0, pos + 1);
            env[std::move(key)] = std::move(e);
        }
        v.env = std::move(env);
    }

    if (auto it = j.find("timeout"); it != j.end()) {
        v.timeout = it->get<int>();
        if (v.timeout.value() <= 0) {
            throw std::runtime_error("hook timeout must be greater than zero");
        }
    }
}

void to_json(nlohmann::json &j, const config::hooks_t::hook_t &v)
{
    j = nlohmann::json::object({ { "path", v.path.string() } });

    if (v.args) {
        j["args"] = *v.args;
    }

    if (v.env) {
        nlohmann::json::array_t env_arr;
        env_arr.reserve(v.env->size());
        std::transform(v.env->begin(),
                       v.env->end(),
                       std::back_inserter(env_arr),
                       [](const auto &pair) {
                           return pair.first + "=" + pair.second;
                       });
        j["env"] = std::move(env_arr);
    }

    if (v.timeout) {
        j["timeout"] = *v.timeout;
    }
}

// --- hooks_t ---

void from_json(const nlohmann::json &j, config::hooks_t &v)
{
    auto get_hooks = [&j](const char *key) -> std::optional<std::vector<config::hooks_t::hook_t>> {
        auto it = j.find(key);
        if (it == j.end()) {
            return std::nullopt;
        }
        return it->get<std::vector<config::hooks_t::hook_t>>();
    };

    v.prestart = get_hooks("prestart");
    v.create_runtime = get_hooks("createRuntime");
    v.create_container = get_hooks("createContainer");
    v.start_container = get_hooks("startContainer");
    v.poststart = get_hooks("poststart");
    v.poststop = get_hooks("poststop");
}

void to_json(nlohmann::json &j, const config::hooks_t &v)
{
    j = nlohmann::json::object();
    if (v.prestart) {
        j["prestart"] = *v.prestart;
    }
    if (v.create_runtime) {
        j["createRuntime"] = *v.create_runtime;
    }
    if (v.create_container) {
        j["createContainer"] = *v.create_container;
    }
    if (v.start_container) {
        j["startContainer"] = *v.start_container;
    }
    if (v.poststart) {
        j["poststart"] = *v.poststart;
    }
    if (v.poststop) {
        j["poststop"] = *v.poststop;
    }
}

// --- mount_t ---

void from_json(const nlohmann::json &j, config::mount_t &v)
{
    j.at("destination").get_to(v.destination);

    if (auto it = j.find("source"); it != j.end()) {
        v.source = it->get<std::string>();
    }
    if (auto it = j.find("type"); it != j.end()) {
        v.type = it->get<std::string>();
    }
    if (auto it = j.find("uidMappings"); it != j.end()) {
        v.uid_mappings = it->get<std::vector<config::linux_t::id_mapping_t>>();
    }
    if (auto it = j.find("gidMappings"); it != j.end()) {
        v.gid_mappings = it->get<std::vector<config::linux_t::id_mapping_t>>();
    }

    if (auto it = j.find("options"); it != j.end()) {
        auto options = it->get<std::vector<std::string>>();
        std::tie(v.flags, v.propagation_flags, v.extension_flags, v.data) =
          parse_mount_options(options);
    }
}

void to_json(nlohmann::json &j, const config::mount_t &v)
{
    j = nlohmann::json::object({ { "destination", v.destination.string() } });

    if (v.source) {
        j["source"] = *v.source;
    }
    if (v.type) {
        j["type"] = *v.type;
    }
    if (v.uid_mappings) {
        j["uidMappings"] = *v.uid_mappings;
    }
    if (v.gid_mappings) {
        j["gidMappings"] = *v.gid_mappings;
    }

    auto options = to_mount_options(v.flags, v.propagation_flags, v.extension_flags, v.data);
    if (!options.empty()) {
        j["options"] = std::move(options);
    }
}

// --- root_t ---

void from_json(const nlohmann::json &j, config::root_t &v)
{
    j.at("path").get_to(v.path);
    v.readonly = j.value("readonly", false);
}

void to_json(nlohmann::json &j, const config::root_t &v)
{
    j = nlohmann::json::object({ { "path", v.path.string() }, { "readonly", v.readonly } });
}

// --- config (top-level) ---

void from_json(const nlohmann::json &j, config &v)
{
    auto semver = linyaps_box::utils::semver(j.at("ociVersion").get_ref<const std::string &>());
    if (!linyaps_box::utils::semver(config::oci_version).is_compatible_with(semver)) {
        throw std::runtime_error("unsupported OCI version: " + semver.to_string());
    }

    j.at("process").get_to(v.process);

    if (auto it = j.find("hostname"); it != j.end()) {
        v.hostname = it->get<std::string>();
    }

    if (auto it = j.find("domainname"); it != j.end()) {
        v.domainname = it->get<std::string>();
    }

    if (auto it = j.find("linux"); it != j.end()) {
        v.linux = it->get<config::linux_t>();
    }

    if (auto it = j.find("hooks"); it != j.end()) {
        v.hooks = it->get<config::hooks_t>();
    }

    if (auto it = j.find("mounts"); it != j.end()) {
        v.mounts = it->get<std::vector<config::mount_t>>();
    }

    if (!j.contains("root")) {
        throw std::runtime_error("root must be specified");
    }
    j.at("root").get_to(v.root);

    if (auto it = j.find("annotations"); it != j.end()) {
        v.annotations = it->get<std::unordered_map<std::string, std::string>>();
    }
}

void to_json(nlohmann::json &j, const config &v)
{
    j = nlohmann::json::object({ { "ociVersion", config::oci_version } });

    j["process"] = v.process;

    if (v.hostname) {
        j["hostname"] = *v.hostname;
    }
    if (v.domainname) {
        j["domainname"] = *v.domainname;
    }
    if (v.linux) {
        j["linux"] = *v.linux;
    }
    if (v.hooks) {
        j["hooks"] = *v.hooks;
    }
    if (!v.mounts.empty()) {
        j["mounts"] = v.mounts;
    }
    j["root"] = v.root;
    if (v.annotations) {
        j["annotations"] = *v.annotations;
    }
}

} // namespace linyaps_box

linyaps_box::config linyaps_box::config::parse(std::string_view content)
{
    return nlohmann::json::parse(content).get<linyaps_box::config>();
}

auto linyaps_box::to_string_view(linyaps_box::config::linux_t::namespace_t::type type) noexcept
  -> std::string_view
{
    for (const auto &[val, name] : namespace_type_to_string) {
        if (val == type) {
            return name;
        }
    }

    __builtin_unreachable();
}

void linyaps_box::validate_namespace_path(const linyaps_box::config::linux_t::namespace_t &ns)
{
    if (!ns.path) {
        return;
    }

    auto fs_path = ns.path.value();
    std::error_code ec;
    auto target = std::filesystem::read_symlink(fs_path, ec);
    if (ec) {
        throw std::runtime_error("namespace path " + fs_path.string()
                                 + " does not exist or is not accessible");
    }

    auto target_str = target.string();
    auto colon_pos = target_str.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("namespace path " + fs_path.string()
                                 + " does not appear to be a namespace file");
    }

    auto ns_type_from_path = target_str.substr(0, colon_pos);
    auto expected_str = linyaps_box::to_string_view(ns.type_);
    if (ns_type_from_path != expected_str) {
        throw std::runtime_error("namespace path " + fs_path.string() + " is associated with '"
                                 + ns_type_from_path + "' namespace, not '"
                                 + std::string(expected_str) + "'");
    }
}

auto linyaps_box::to_string_view(linyaps_box::config::process_t::rlimit_t::type_t type) noexcept
  -> std::string_view
{
    for (const auto &[val, name] : rlimit_type_to_string) {
        if (val == type) {
            return name;
        }
    }

    __builtin_unreachable();
}
