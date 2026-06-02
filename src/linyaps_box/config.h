// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <linux/ioprio.h>
#include <numaif.h>
#include <sys/mount.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>
#include <sys/types.h>

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
#  include <seccomp.h>
#endif

// Compatible with linux kernel which is under 5.10
#ifndef MS_NOSYMFOLLOW
#  define LINGYAPS_MS_NOSYMFOLLOW 256
#else
#  define LINGYAPS_MS_NOSYMFOLLOW MS_NOSYMFOLLOW
#endif

namespace linyaps_box {

struct config
{
    static constexpr auto oci_version = "1.3.0";

    static auto parse(std::string_view content) -> config;

    struct process_t
    {
        bool terminal{ false };

        struct console_size_t
        {
            unsigned short height{ 0 };
            unsigned short width{ 0 };
        };

        std::optional<console_size_t> console_size;

        std::filesystem::path cwd;
        std::optional<std::vector<std::string>> env;
        std::vector<std::string> args;

        struct rlimit_t
        {
            enum class type_t : uint8_t {
                AS = RLIMIT_AS,
                CORE = RLIMIT_CORE,
                CPU = RLIMIT_CPU,
                DATA = RLIMIT_DATA,
                FSIZE = RLIMIT_FSIZE,
                MEMLOCK = RLIMIT_MEMLOCK,
                MSGQUEUE = RLIMIT_MSGQUEUE,
                NICE = RLIMIT_NICE,
                NOFILE = RLIMIT_NOFILE,
                NPROC = RLIMIT_NPROC,
                RSS = RLIMIT_RSS,
                RTPRIO = RLIMIT_RTPRIO,
                RTTIME = RLIMIT_RTTIME,
                SIGPENDING = RLIMIT_SIGPENDING,
                STACK = RLIMIT_STACK,
            };

            type_t type;
            uint64_t soft;
            uint64_t hard;
        };

        std::optional<std::vector<rlimit_t>> rlimits;
        std::optional<std::string> apparmor_profile;

        // TODO: not use cap_value_t directly
#ifdef LINYAPS_BOX_ENABLE_CAP
        struct capabilities_t
        {
            std::optional<std::vector<cap_value_t>> effective;
            std::optional<std::vector<cap_value_t>> bounding;
            std::optional<std::vector<cap_value_t>> inheritable;
            std::optional<std::vector<cap_value_t>> permitted;
            std::optional<std::vector<cap_value_t>> ambient;
        };

        std::optional<capabilities_t> capabilities;
#endif

        std::optional<bool> no_new_privileges;
        std::optional<int> oom_score_adj;

        struct scheduler_t
        {
            enum class policy_t : uint8_t {
                OTHER = SCHED_OTHER,
                FIFO = SCHED_FIFO,
                RR = SCHED_RR,
                BATCH = SCHED_BATCH,
                ISO = SCHED_ISO,
                IDLE = SCHED_IDLE,
                DEADLINE = SCHED_DEADLINE,
            };

            policy_t policy;
            std::optional<int32_t> nice;
            std::optional<int32_t> priority;

            enum class flag_t : uint8_t {
                RESET_ON_FORK = SCHED_FLAG_RESET_ON_FORK,
                RECLAIM = SCHED_FLAG_RECLAIM,
                DL_OVERRUN = SCHED_FLAG_DL_OVERRUN,
                KEEP_POLICY = SCHED_FLAG_KEEP_POLICY,
                KEEP_PARAMS = SCHED_FLAG_KEEP_PARAMS,
                UTIL_CLAMP_MIN = SCHED_FLAG_UTIL_CLAMP_MIN,
                UTIL_CLAMP_MAX = SCHED_FLAG_UTIL_CLAMP_MAX,
            };
            std::optional<int> flags;

            std::optional<uint64_t> runtime;
            std::optional<uint64_t> deadline;
            std::optional<uint64_t> period;
        };

        std::optional<scheduler_t> scheduler;
        std::optional<std::string> selinux_label;

        struct io_priority_t
        {
            enum class class_t : uint8_t {
                RT = IOPRIO_CLASS_RT,
                BEST_EFFORT = IOPRIO_CLASS_BE,
                IDLE = IOPRIO_CLASS_IDLE,
            };

            class_t class_;
            int priority;
        };

        std::optional<io_priority_t> io_priority;

        struct exec_cpu_affinity_t
        {
            std::optional<std::string> initial;
            std::optional<std::string> final;
        };

        std::optional<exec_cpu_affinity_t> exec_cpu_affinity;

        struct user_t
        {
            uid_t uid;
            gid_t gid;
            std::optional<mode_t> umask;
            std::optional<std::vector<gid_t>> additional_gids;
        };

        user_t user;
    };

    // this is required when start is called
    process_t process;

    std::optional<std::string> hostname;
    std::optional<std::string> domainname;

    struct linux_t
    {
        struct namespace_t
        {
            enum class type : unsigned int {
                NONE = 0,
                IPC = CLONE_NEWIPC,
                UTS = CLONE_NEWUTS,
                MOUNT = CLONE_NEWNS,
                PID = CLONE_NEWPID,
                NET = CLONE_NEWNET,
                USER = CLONE_NEWUSER,
                CGROUP = CLONE_NEWCGROUP,
                TIME = CLONE_NEWTIME,
            };

            type type_;
            std::optional<std::filesystem::path> path;
        };

        std::optional<std::vector<namespace_t>> namespaces;

        struct id_mapping_t
        {
            uid_t host_id;
            uid_t container_id;
            size_t size;
        };

        std::optional<std::vector<id_mapping_t>> uid_mappings;
        std::optional<std::vector<id_mapping_t>> gid_mappings;

        struct time_offset_t
        {
            int64_t secs;
            uint32_t nanosecs;
        };

        std::optional<std::unordered_map<std::string, time_offset_t>> time_offsets;

        struct device_t
        {
            std::string type;
            std::filesystem::path path;
            // REQUIRED unless type is 'p'
            std::optional<uint32_t> major;
            std::optional<uint32_t> minor;
            std::optional<uint32_t> mode;
            std::optional<uint32_t> uid;
            std::optional<uint32_t> gid;
        };

        std::optional<std::vector<device_t>> devices;

        struct network_device_t
        {
            std::optional<std::string> name;
        };

        std::optional<std::vector<network_device_t>> network_devices;
        std::optional<std::string> cgroups_path;

        struct allowed_device_t
        {
            bool allow;
            std::optional<std::string> type;
            std::optional<int64_t> major;
            std::optional<int64_t> minor;
            std::optional<std::string> access;
        };

        std::optional<std::vector<allowed_device_t>> allowed_devices;

        struct memory_t
        {
            std::optional<int64_t> limit;
            std::optional<int64_t> reservation;
            std::optional<int64_t> swap;
            std::optional<int64_t> kernel;
            std::optional<int64_t> kernel_tcp;
            std::optional<uint64_t> swappiness;
            std::optional<bool> disable_OOM_killer;
            std::optional<bool> use_hierarchy;
            std::optional<bool> check_before_update;
        };

        std::optional<memory_t> memory;

        struct cpu_t
        {
            enum class idle_t : uint8_t { NONE = 0, IDLE = 1 };
            std::optional<uint64_t> shares;
            std::optional<int64_t> quota;
            std::optional<uint64_t> burst;
            std::optional<uint64_t> period;
            std::optional<int64_t> realtime_runtime;
            std::optional<uint64_t> realtime_period;
            std::optional<std::vector<uint>> cpus;
            std::optional<std::string> mems;
            std::optional<idle_t> idle;
        };

        std::optional<cpu_t> cpu;

        struct block_io_t
        {
            std::optional<uint16_t> weight;
            std::optional<uint16_t> leaf_weight;

            struct weight_device_t
            {
                int64_t major;
                int64_t minor;
                std::optional<uint16_t> weight;
                std::optional<uint16_t> leaf_weight;
            };

            std::optional<std::vector<weight_device_t>> weight_devices;

            struct throttle_device_t
            {
                int64_t major;
                int64_t minor;
                uint64_t rate;
            };

            std::optional<std::vector<throttle_device_t>> throttle_read_bps_device;
            std::optional<std::vector<throttle_device_t>> throttle_write_bps_device;
            std::optional<std::vector<throttle_device_t>> throttle_read_iops_device;
            std::optional<std::vector<throttle_device_t>> throttle_write_iops_device;
        };

        std::optional<block_io_t> block_io;

        struct hugepage_limit_t
        {
            std::string page_size;
            uint64_t limit;
        };

        std::optional<std::vector<hugepage_limit_t>> hugepage_limits;

        struct network_t
        {
            std::optional<uint32_t> class_id;

            struct priority_t
            {
                std::string name;
                uint32_t priority;
            };

            std::optional<std::vector<priority_t>> priorities;
        };

        std::optional<network_t> network;

        struct pids_t
        {
            std::optional<int64_t> limit;
        };

        std::optional<pids_t> pids;

        struct rdma_t
        {
            std::optional<uint32_t> hca_handles;
            std::optional<uint32_t> hca_objects;
        };

        std::optional<std::unordered_map<std::string, rdma_t>> rdma;

        std::optional<std::unordered_map<std::string, std::string>> unified;

        struct intel_rdt_t
        {
            std::optional<std::string> clos_id;
            std::optional<std::string> l3_cache_schema;
            std::optional<std::string> memory_bandwidth_schema;
            std::optional<std::vector<std::string>> schemata;
            std::optional<bool> enable_monitoring;
        };

        std::optional<intel_rdt_t> intel_rdt;

        struct memory_policy_t
        {
            enum class mode_t : uint8_t {
                DEFAULT = MPOL_DEFAULT,
                BIND = MPOL_BIND,
                INTERLEAVE = MPOL_INTERLEAVE,
                WEIGHTED_INTERLEAVE = MPOL_WEIGHTED_INTERLEAVE,
                PREFERRED = MPOL_PREFERRED,
                PREFERRED_MANY = MPOL_PREFERRED_MANY,
                LOCAL = MPOL_LOCAL,
            };

            mode_t mode;
            std::optional<std::vector<int>> nodes;

            enum class flag_t : uint16_t {
                NUMA_BALANCING = MPOL_F_NUMA_BALANCING,
                RELATIVE_NODES = MPOL_F_RELATIVE_NODES,
                STATIC_NODES = MPOL_F_STATIC_NODES,
            };
            std::optional<flag_t> flags;
        };

        std::optional<memory_policy_t> memory_policy;

        std::optional<std::unordered_map<std::string, std::string>> sysctl;

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
        struct seccomp_t
        {
            std::string default_action;
            std::optional<uint> default_errno_ret;

            enum class arch_t : uint32_t {
                X86 = SCMP_ARCH_X86,
                X86_64 = SCMP_ARCH_X86_64,
                X32 = SCMP_ARCH_X32,
                ARM = SCMP_ARCH_ARM,
                AARCH64 = SCMP_ARCH_AARCH64,
                MIPS = SCMP_ARCH_MIPS,
                MIPS64 = SCMP_ARCH_MIPS64,
                MIPS64N32 = SCMP_ARCH_MIPS64N32,
                MIPSEL = SCMP_ARCH_MIPSEL,
                MIPSEL64 = SCMP_ARCH_MIPSEL64,
                MIPSEL64N32 = SCMP_ARCH_MIPSEL64N32,
                PPC = SCMP_ARCH_PPC,
                PPC64 = SCMP_ARCH_PPC64,
                PPC64LE = SCMP_ARCH_PPC64LE,
                S390 = SCMP_ARCH_S390,
                S390X = SCMP_ARCH_S390X,
                PARISC = SCMP_ARCH_PARISC,
                PARISC64 = SCMP_ARCH_PARISC64,
                RISCV64 = SCMP_ARCH_RISCV64,
                LOONGARCH64 = SCMP_ARCH_LOONGARCH64,
                M68K = SCMP_ARCH_M68K,
                SH = SCMP_ARCH_SH,
                SHEB = SCMP_ARCH_SHEB
            };
            std::optional<std::vector<arch_t>> architectures;

            enum class flag_t : std::uint8_t {
                TSYNC = SECCOMP_FILTER_FLAG_TSYNC,
                LOG = SECCOMP_FILTER_FLAG_LOG,
                SPEC_ALLOW = SECCOMP_FILTER_FLAG_SPEC_ALLOW,
                WAIT_KILLABLE_RECV = SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV,
            };
            std::optional<flag_t> flags;

            std::optional<std::filesystem::path> listener_path;
            std::optional<std::string> listener_metadata;

            struct syscall_t
            {
                std::vector<std::string> name;
                std::string action;
                std::optional<uint> errno_ret;

                struct arg_t
                {
                    uint index;
                    uint64_t value;
                    uint64_t value_two;

                    enum class op_t : uint8_t {
                        EQ = SCMP_CMP_EQ,
                        NE = SCMP_CMP_NE,
                        LT = SCMP_CMP_LT,
                        LE = SCMP_CMP_LE,
                        GT = SCMP_CMP_GT,
                        GE = SCMP_CMP_GE,
                        MASKED_EQ = SCMP_CMP_MASKED_EQ
                    };
                    op_t op;
                };

                std::optional<std::vector<arg_t>> args;
            };

            std::optional<std::vector<syscall_t>> syscalls;
        };

        std::optional<seccomp_t> seccomp;
#endif
        enum class rootfs_propagation_t : uint {
            PRIVATE = MS_PRIVATE,
            SHARED = MS_SHARED,
            SLAVE = MS_SLAVE,
            UNBINDABLE = MS_UNBINDABLE
        };
        std::optional<rootfs_propagation_t> rootfs_propagation;

        std::optional<std::vector<std::filesystem::path>> masked_paths;
        std::optional<std::vector<std::filesystem::path>> readonly_paths;

        std::optional<std::string> mount_label;

        struct personality_t
        {
            enum class domain_t : uint8_t { LINUX, LINUX32 };
            domain_t domain;

            std::optional<std::vector<std::string>> flags;
        };

        std::optional<personality_t> personality;
    };

    std::optional<linux_t> linux;

    struct hooks_t
    {
        struct hook_t
        {
            std::filesystem::path path;
            std::optional<std::vector<std::string>> args;
            std::optional<std::unordered_map<std::string, std::string>> env;
            std::optional<int> timeout;
        };

        std::optional<std::vector<hook_t>> prestart;
        std::optional<std::vector<hook_t>> create_runtime;
        std::optional<std::vector<hook_t>> create_container;
        std::optional<std::vector<hook_t>> start_container;
        std::optional<std::vector<hook_t>> poststart;
        std::optional<std::vector<hook_t>> poststop;
    };

    std::optional<hooks_t> hooks;

    struct mount_t
    {
        enum class extension : std::uint8_t { NONE = 0, COPY_SYMLINK };

        std::optional<std::string> source;
        std::filesystem::path destination;
        std::optional<std::string> type;
        std::string data;

        // for id mapped mount
        std::optional<std::vector<linux_t::id_mapping_t>> uid_mappings;
        std::optional<std::vector<linux_t::id_mapping_t>> gid_mappings;
        unsigned long flags{ 0 };
        unsigned long propagation_flags{ 0 };
        extension extension_flags{ 0 };
    };

    std::vector<mount_t> mounts;

    struct root_t
    {
        std::filesystem::path path;
        bool readonly{ false };
    };

    root_t root;

    std::optional<std::unordered_map<std::string, std::string>> annotations;
};

auto to_string_view(linyaps_box::config::linux_t::namespace_t::type type) noexcept
  -> std::string_view;
auto to_string_view(linyaps_box::config::process_t::rlimit_t::type_t type) noexcept
  -> std::string_view;

void validate_namespace_path(const linyaps_box::config::linux_t::namespace_t &ns);

} // namespace linyaps_box

constexpr linyaps_box::config::mount_t::extension
operator|(linyaps_box::config::mount_t::extension lhs, linyaps_box::config::mount_t::extension rhs)
{
    return static_cast<linyaps_box::config::mount_t::extension>(
      static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(lhs)
      | static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(rhs));
}

constexpr linyaps_box::config::mount_t::extension
operator&(linyaps_box::config::mount_t::extension lhs, linyaps_box::config::mount_t::extension rhs)
{
    return static_cast<linyaps_box::config::mount_t::extension>(
      static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(lhs)
      & static_cast<std::underlying_type_t<linyaps_box::config::mount_t::extension>>(rhs));
}

constexpr linyaps_box::config::mount_t::extension &
operator|=(linyaps_box::config::mount_t::extension &lhs,
           linyaps_box::config::mount_t::extension rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr linyaps_box::config::mount_t::extension &
operator&=(linyaps_box::config::mount_t::extension &lhs,
           linyaps_box::config::mount_t::extension rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

constexpr linyaps_box::config::linux_t::namespace_t::type
operator|(linyaps_box::config::linux_t::namespace_t::type lhs,
          linyaps_box::config::linux_t::namespace_t::type rhs) noexcept
{
    return static_cast<linyaps_box::config::linux_t::namespace_t::type>(
      static_cast<std::underlying_type_t<linyaps_box::config::linux_t::namespace_t::type>>(lhs)
      | static_cast<std::underlying_type_t<linyaps_box::config::linux_t::namespace_t::type>>(rhs));
}

constexpr linyaps_box::config::linux_t::namespace_t::type
operator&(linyaps_box::config::linux_t::namespace_t::type lhs,
          linyaps_box::config::linux_t::namespace_t::type rhs) noexcept
{
    return static_cast<linyaps_box::config::linux_t::namespace_t::type>(
      static_cast<std::underlying_type_t<linyaps_box::config::linux_t::namespace_t::type>>(lhs)
      & static_cast<std::underlying_type_t<linyaps_box::config::linux_t::namespace_t::type>>(rhs));
}

constexpr linyaps_box::config::linux_t::namespace_t::type &
operator|=(linyaps_box::config::linux_t::namespace_t::type &lhs,
           linyaps_box::config::linux_t::namespace_t::type rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr linyaps_box::config::linux_t::namespace_t::type &
operator&=(linyaps_box::config::linux_t::namespace_t::type &lhs,
           linyaps_box::config::linux_t::namespace_t::type rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}
