// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace linyaps_box {

struct oci_config
{
    static constexpr auto version = "1.3.0";

    static auto parse(std::string_view content) -> oci_config;
    static auto parse(const std::filesystem::path &path) -> oci_config;

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
            enum class type_t : std::uint8_t {
                AS,
                CORE,
                CPU,
                DATA,
                FSIZE,
                LOCKS,
                MEMLOCK,
                MSGQUEUE,
                NICE,
                NOFILE,
                NPROC,
                RSS,
                RTPRIO,
                RTTIME,
                SIGPENDING,
                STACK,
            };

            type_t type;
            uint64_t soft;
            uint64_t hard;
        };

        std::optional<std::vector<rlimit_t>> rlimits;
        std::optional<std::string> apparmor_profile;

        struct capabilities_t
        {
            std::optional<std::vector<std::string>> effective;
            std::optional<std::vector<std::string>> bounding;
            std::optional<std::vector<std::string>> inheritable;
            std::optional<std::vector<std::string>> permitted;
            std::optional<std::vector<std::string>> ambient;
        };

        std::optional<capabilities_t> capabilities;

        std::optional<bool> no_new_privileges;
        std::optional<int> oom_score_adj;

        struct scheduler_t
        {
            enum class policy_t : uint8_t { OTHER, FIFO, RR, BATCH, ISO, IDLE, DEADLINE };

            policy_t policy;
            std::optional<int32_t> nice;
            std::optional<int32_t> priority;

            enum class flag_t : std::uint8_t {
                RESET_ON_FORK,
                RECLAIM,
                DL_OVERRUN,
                KEEP_POLICY,
                KEEP_PARAMS,
                UTIL_CLAMP_MIN,
                UTIL_CLAMP_MAX,
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
                RT,
                BEST_EFFORT,
                IDLE,
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
            std::optional<std::filesystem::perms> umask;
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
            COPY_SYMLINK = (1U << 0),
            TMPCOPYUP = (1U << 1),
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

        std::optional<std::vector<oci_config::id_mapping_t>> uid_mappings;
        std::optional<std::vector<oci_config::id_mapping_t>> gid_mappings;
    };

    std::vector<mount_t> mounts;

    struct linux_t
    {
        struct namespace_t
        {
            enum class type : std::uint8_t {
                NONE = 0U,
                IPC = (1U << 0),
                UTS = (1U << 1),
                MOUNT = (1U << 2),
                PID = (1U << 3),
                NET = (1U << 4),
                USER = (1U << 5),
                CGROUP = (1U << 6),
                TIME = (1U << 7),
            };

            type type_;
            std::optional<std::filesystem::path> path;
        };

        std::optional<std::vector<namespace_t>> namespaces;

        std::optional<std::vector<oci_config::id_mapping_t>> uid_mappings;
        std::optional<std::vector<oci_config::id_mapping_t>> gid_mappings;

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
                enum class idle_t : uint8_t { NONE, IDLE };
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
                DEFAULT,
                BIND,
                INTERLEAVE,
                WEIGHTED_INTERLEAVE,
                PREFERRED,
                PREFERRED_MANY,
                LOCAL,
            };

            mode_t mode;
            std::optional<std::vector<unsigned int>> nodes;

            enum class flag_t : std::uint8_t {
                NUMA_BALANCING = 1,
                RELATIVE_NODES = 2,
                STATIC_NODES = 4,
            };
            std::optional<flag_t> flags;
        };

        std::optional<memory_policy_t> memory_policy;

        std::optional<std::unordered_map<std::string, std::string>> sysctl;

        struct seccomp_t
        {
            enum class action_t : std::uint8_t {
                ALLOW,
                ERRNO,
                KILL,
                KILL_PROCESS,
                KILL_THREAD,
                LOG,
                NOTIFY,
                TRACE,
                TRAP
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
                NONE = 0,
                TSYNC = 1,
                LOG = 2,
                SPEC_ALLOW = 4,
                WAIT_KILLABLE_RECV = 8,
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
                    uint index{ 0 };
                    uint64_t value{ 0 };
                    std::optional<uint64_t> value_two;

                    enum class op_t : uint8_t {
                        EQ,
                        NE,
                        LT,
                        LE,
                        GT,
                        GE,
                        MASKED_EQ,
                    };
                    op_t op;
                };

                std::optional<std::vector<arg_t>> args;
            };

            std::optional<std::vector<syscall_t>> syscalls;
        };

        std::optional<seccomp_t> seccomp;

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

LINYAPS_ENABLE_BITMASK_ENUM(oci_config::process_t::scheduler_t::flag_t);
LINYAPS_ENABLE_BITMASK_ENUM(oci_config::mount_t::extension);
LINYAPS_ENABLE_BITMASK_ENUM(oci_config::linux_t::namespace_t::type);
LINYAPS_ENABLE_BITMASK_ENUM(oci_config::linux_t::memory_policy_t::flag_t);
LINYAPS_ENABLE_BITMASK_ENUM(oci_config::linux_t::seccomp_t::flag_t);

auto to_string_view(oci_config::linux_t::namespace_t::type type) noexcept -> std::string_view;

auto to_string_view(oci_config::process_t::rlimit_t::type_t type) noexcept -> std::string_view;

void validate_namespace_path(const oci_config::linux_t::namespace_t &ns);

void from_json(const nlohmann::json &j, oci_config &v);

void from_json(const nlohmann::json &j, oci_config::process_t &v);

} // namespace linyaps_box
