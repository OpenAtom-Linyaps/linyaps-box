// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <linux/ioprio.h>
#include <linux/mempolicy.h>
#include <linux/sched.h>
#include <sys/mount.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>
#include <sys/types.h>

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
#  include <seccomp.h>
#endif

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

#include "linyaps_box/utils/enum_traits.h"
#include "nlohmann/json_fwd.hpp"

// clang-format off
#include "linyaps_box/config/kernel_fallbacks.h"
// clang-format on

namespace linyaps_box {

struct Config
{
    static constexpr auto oci_version = "1.3.0";

    static auto parse(std::string_view content) -> Config;
    static auto parse(const std::filesystem::path &path) -> Config;

    struct process_t
    {
        std::optional<bool> terminal;

        struct console_size_t
        {
            unsigned short height;
            unsigned short width;
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
                LOCKS = RLIMIT_LOCKS,
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
#endif // LINYAPS_BOX_ENABLE_CAP

        std::optional<bool> no_new_privileges;
        std::optional<int> oom_score_adj;

        struct scheduler_t
        {
            enum class policy_t : uint8_t {
                OTHER = SCHED_OTHER,
                FIFO = SCHED_FIFO,
                RR = SCHED_RR,
                BATCH = SCHED_BATCH,
                ISO = kernel::sched_iso,
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
                UTIL_CLAMP_MIN = static_cast<uint8_t>(kernel::sched_flag_util_clamp_min),
                UTIL_CLAMP_MAX = static_cast<uint8_t>(kernel::sched_flag_util_clamp_max),
            };

            std::optional<flag_t> flags;

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

        // TODO: maybe user should be optional?
        user_t user;
    };

    std::optional<process_t> process;

    std::optional<std::string> hostname;
    std::optional<std::string> domainname;

    struct id_mapping_t
    {
        uid_t host_id;
        uid_t container_id;
        size_t size;
    };

    struct mount_t
    {
        enum class extension : std::uint8_t {
            NONE = 0,
            COPY_SYMLINK = (1U << 0U),
            TMPCOPYUP = (1U << 1U),
        };

        enum class idmap_type : std::uint8_t { IDMAP, RIDMAP };

        // VFS mount(2) flags — passed directly to mount()'s 4th argument.
        // Does NOT contain propagation flags; see propagation_flags.
        unsigned long vfs_flags{ 0 };

        // Propagation flags — applied via separate mount(2) call after the main mount.
        // Bitmask of MS_PRIVATE, MS_SHARED, MS_SLAVE, MS_UNBINDABLE, optionally OR-ed with MS_REC.
        unsigned long propagation_flags{ 0 };

        // Recursive mount_setattr attributes — applied via mount_setattr(AT_RECURSIVE).
        // TODO: implement mount_setattr.
        struct recursive_attr
        {
            uint64_t set{ 0 };
            uint64_t clr{ 0 };
        };

        std::optional<recursive_attr> rec_attr;

        // Runtime extension flags — bitmap of extension values.
        extension extension_flags{ extension::NONE };

        // ID mapping type — idmap/ridmap, mutually exclusive.
        std::optional<idmap_type> idmap;

        std::optional<std::string> source;
        std::filesystem::path destination;
        std::optional<std::string> type;
        std::string data;

        std::optional<std::vector<Config::id_mapping_t>> uid_mappings;
        std::optional<std::vector<Config::id_mapping_t>> gid_mappings;
    };

    std::vector<mount_t> mounts;

    struct linux_t
    {
        struct namespace_t
        {
            enum class type : unsigned int {
                IPC = CLONE_NEWIPC,
                UTS = CLONE_NEWUTS,
                MOUNT = CLONE_NEWNS,
                PID = CLONE_NEWPID,
                NET = CLONE_NEWNET,
                USER = CLONE_NEWUSER,
                CGROUP = CLONE_NEWCGROUP,
                TIME = kernel::clone_newtime,
            };

            type type_;
            std::optional<std::filesystem::path> path;
        };

        std::optional<std::vector<namespace_t>> namespaces;

        std::optional<std::vector<Config::id_mapping_t>> uid_mappings;
        std::optional<std::vector<Config::id_mapping_t>> gid_mappings;

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

        std::optional<std::unordered_map<std::string, network_device_t>> network_devices;
        std::optional<std::string> cgroups_path;

        struct resources_t
        {
            struct device_t
            {
                bool allow;
                std::optional<std::string> type;
                std::optional<int64_t> major;
                std::optional<int64_t> minor;
                std::optional<std::string> access;
            };

            std::optional<std::vector<device_t>> devices;

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
                std::optional<std::vector<unsigned int>> cpus;
                std::optional<std::vector<unsigned int>> mems;
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
        };

        std::optional<resources_t> resources;

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
                WEIGHTED_INTERLEAVE = static_cast<int>(kernel::mpol_weighted_interleave),
                PREFERRED = MPOL_PREFERRED,
                PREFERRED_MANY = static_cast<int>(kernel::mpol_preferred_many),
                LOCAL = MPOL_LOCAL,
            };

            mode_t mode;
            std::optional<std::vector<unsigned int>> nodes;

            enum class flag_t : uint16_t {
                NUMA_BALANCING = static_cast<uint16_t>(kernel::mpol_f_numa_balancing),
                RELATIVE_NODES = static_cast<uint16_t>(MPOL_F_RELATIVE_NODES),
                STATIC_NODES = static_cast<uint16_t>(MPOL_F_STATIC_NODES),
            };
            std::optional<flag_t> flags;
        };

        std::optional<memory_policy_t> memory_policy;

        std::optional<std::unordered_map<std::string, std::string>> sysctl;

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
        struct seccomp_t
        {
            enum class action_t : std::uint32_t {
                ALLOW = SCMP_ACT_ALLOW,
                ERRNO = SCMP_ACT_ERRNO(0),
                KILL = SCMP_ACT_KILL,
                KILL_PROCESS = static_cast<std::uint32_t>(kernel::scmp_act_kill_process),
                KILL_THREAD = static_cast<std::uint32_t>(kernel::scmp_act_kill_thread),
                LOG = static_cast<std::uint32_t>(kernel::scmp_act_log),
                NOTIFY = static_cast<std::uint32_t>(kernel::scmp_act_notify),
                TRACE = SCMP_ACT_TRACE(0),
                TRAP = SCMP_ACT_TRAP
            };

            action_t default_action;
            std::optional<uint> default_errno_ret;

            // TODO: currently we only parsing seccomp flags
            // using it when we supports seccomp fully
            enum class arch_t : uint8_t {
                X86,
                X86_64,
                X32,
                ARM,
                AARCH64,
                MIPS,
                MIPS64,
                MIPS64N32,
                MIPSEL,
                MIPSEL64,
                MIPSEL64N32,
                PPC,
                PPC64,
                PPC64LE,
                S390,
                S390X,
                PARISC,
                PARISC64,
                RISCV64,
                LOONGARCH64,
                M68K,
                SH,
                SHEB,
            };
            std::optional<std::vector<arch_t>> architectures;

            enum class flag_t : std::uint8_t {
                TSYNC = SECCOMP_FILTER_FLAG_TSYNC,
                LOG = static_cast<std::uint8_t>(kernel::seccomp_filter_flag_log),
                SPEC_ALLOW = static_cast<std::uint8_t>(kernel::seccomp_filter_flag_spec_allow),
                WAIT_KILLABLE_RECV =
                  static_cast<std::uint8_t>(kernel::seccomp_filter_flag_wait_killable_recv),
            };
            std::optional<flag_t> flags;

            std::optional<std::filesystem::path> listener_path;
            std::optional<std::string> listener_metadata;

            struct syscall_t
            {
                std::vector<std::string> names;
                action_t action;
                std::optional<uint> errno_ret;

                struct arg_t
                {
                    uint index;
                    uint64_t value;
                    std::optional<uint64_t> value_two;

                    enum class op_t : uint8_t {
                        EQ = SCMP_CMP_EQ,
                        NE = SCMP_CMP_NE,
                        LT = SCMP_CMP_LT,
                        LE = SCMP_CMP_LE,
                        GT = SCMP_CMP_GT,
                        GE = SCMP_CMP_GE,
                        MASKED_EQ = SCMP_CMP_MASKED_EQ,
                    };
                    op_t op;
                };

                std::optional<std::vector<arg_t>> args;
            };

            std::optional<std::vector<syscall_t>> syscalls;
        };

        std::optional<seccomp_t> seccomp;
#endif

        unsigned long rootfs_propagation{ 0 };

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
            std::optional<std::vector<std::string>> env;
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

    struct root_t
    {
        std::filesystem::path path;
        bool readonly{ false };
    };

    std::optional<root_t> root;

    std::optional<std::unordered_map<std::string, std::string>> annotations;
};

auto to_string_view(Config::linux_t::namespace_t::type type) noexcept -> std::string_view;

auto to_string_view(Config::process_t::rlimit_t::type_t type) noexcept -> std::string_view;

void validate_namespace_path(const Config::linux_t::namespace_t &ns);

void from_json(const nlohmann::json &j, Config &v);

template <>
struct utils::is_bitmask_enum<Config::linux_t::namespace_t::type> : std::true_type
{
};

template <>
struct utils::is_bitmask_enum<Config::mount_t::extension> : std::true_type
{
};

template <>
struct utils::is_bitmask_enum<Config::process_t::scheduler_t::flag_t> : std::true_type
{
};

template <>
struct utils::is_bitmask_enum<Config::linux_t::memory_policy_t::flag_t> : std::true_type
{
};

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
template <>
struct utils::is_bitmask_enum<Config::linux_t::seccomp_t::flag_t> : std::true_type
{
};
#endif

using utils::operator|;
using utils::operator&;
using utils::operator^;
using utils::operator~;
using utils::operator|=;
using utils::operator&=;
using utils::operator^=;

} // namespace linyaps_box
