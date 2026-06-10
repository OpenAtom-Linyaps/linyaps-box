// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config.h"

#include "linyaps_box/config/mount_options.h"
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/semver.h"
#include "linyaps_box/utils/utils.h"
#include "nlohmann/json.hpp"

#include <bitset>
#include <fstream>
#include <limits>

namespace nlohmann {
template <typename T>
struct adl_serializer<std::optional<T>>
{

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

namespace mo = linyaps_box::config::mount_options;

constexpr auto rlimit_type_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::process_t::rlimit_t::type_t, 16>{
      { { { linyaps_box::Config::process_t::rlimit_t::type_t::AS, "RLIMIT_AS" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::CORE, "RLIMIT_CORE" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::CPU, "RLIMIT_CPU" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::DATA, "RLIMIT_DATA" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::FSIZE, "RLIMIT_FSIZE" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::LOCKS, "RLIMIT_LOCKS" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::MEMLOCK, "RLIMIT_MEMLOCK" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::MSGQUEUE, "RLIMIT_MSGQUEUE" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::NICE, "RLIMIT_NICE" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::NOFILE, "RLIMIT_NOFILE" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::NPROC, "RLIMIT_NPROC" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::RSS, "RLIMIT_RSS" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::RTPRIO, "RLIMIT_RTPRIO" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::RTTIME, "RLIMIT_RTTIME" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::SIGPENDING, "RLIMIT_SIGPENDING" },
          { linyaps_box::Config::process_t::rlimit_t::type_t::STACK, "RLIMIT_STACK" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(rlimit_type_table));

constexpr auto scheduler_policy_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::process_t::scheduler_t::policy_t, 7>{
      { { { linyaps_box::Config::process_t::scheduler_t::policy_t::OTHER, "SCHED_OTHER" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::FIFO, "SCHED_FIFO" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::RR, "SCHED_RR" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::BATCH, "SCHED_BATCH" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::ISO, "SCHED_ISO" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::IDLE, "SCHED_IDLE" },
          { linyaps_box::Config::process_t::scheduler_t::policy_t::DEADLINE, "SCHED_DEADLINE" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(scheduler_policy_table));

constexpr auto scheduler_flag_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::process_t::scheduler_t::flag_t, 7>{ { {
    { linyaps_box::Config::process_t::scheduler_t::flag_t::RESET_ON_FORK,
      "SCHED_FLAG_RESET_ON_FORK" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::RECLAIM, "SCHED_FLAG_RECLAIM" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::DL_OVERRUN, "SCHED_FLAG_DL_OVERRUN" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::KEEP_POLICY, "SCHED_FLAG_KEEP_POLICY" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::KEEP_PARAMS, "SCHED_FLAG_KEEP_PARAMS" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::UTIL_CLAMP_MIN,
      "SCHED_FLAG_UTIL_CLAMP_MIN" },
    { linyaps_box::Config::process_t::scheduler_t::flag_t::UTIL_CLAMP_MAX,
      "SCHED_FLAG_UTIL_CLAMP_MAX" },
  } } };

static_assert(linyaps_box::utils::verify_enum_table(scheduler_flag_table));

constexpr auto io_priority_class_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::process_t::io_priority_t::class_t, 3>{
      { { { linyaps_box::Config::process_t::io_priority_t::class_t::RT, "IOPRIO_CLASS_RT" },
          { linyaps_box::Config::process_t::io_priority_t::class_t::BEST_EFFORT,
            "IOPRIO_CLASS_BE" },
          { linyaps_box::Config::process_t::io_priority_t::class_t::IDLE, "IOPRIO_CLASS_IDLE" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(io_priority_class_table));

constexpr auto personality_domain_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::personality_t::domain_t, 2>{
      { { { linyaps_box::Config::linux_t::personality_t::domain_t::LINUX, "LINUX" },
          { linyaps_box::Config::linux_t::personality_t::domain_t::LINUX32, "LINUX32" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(personality_domain_table));

constexpr auto memory_policy_mode_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::memory_policy_t::mode_t, 7>{
      { { { linyaps_box::Config::linux_t::memory_policy_t::mode_t::DEFAULT, "MPOL_DEFAULT" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::BIND, "MPOL_BIND" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::INTERLEAVE, "MPOL_INTERLEAVE" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::WEIGHTED_INTERLEAVE,
            "MPOL_WEIGHTED_INTERLEAVE" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::PREFERRED, "MPOL_PREFERRED" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::PREFERRED_MANY,
            "MPOL_PREFERRED_MANY" },
          { linyaps_box::Config::linux_t::memory_policy_t::mode_t::LOCAL, "MPOL_LOCAL" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(memory_policy_mode_table));

constexpr auto memory_policy_flag_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::memory_policy_t::flag_t, 3>{
      { { { linyaps_box::Config::linux_t::memory_policy_t::flag_t::NUMA_BALANCING,
            "MPOL_F_NUMA_BALANCING" },
          { linyaps_box::Config::linux_t::memory_policy_t::flag_t::RELATIVE_NODES,
            "MPOL_F_RELATIVE_NODES" },
          { linyaps_box::Config::linux_t::memory_policy_t::flag_t::STATIC_NODES,
            "MPOL_F_STATIC_NODES" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(memory_policy_flag_table));

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
constexpr auto seccomp_op_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t,
                                 7>{
      { { { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::EQ, "SCMP_CMP_EQ" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::NE, "SCMP_CMP_NE" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LT, "SCMP_CMP_LT" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::LE, "SCMP_CMP_LE" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GT, "SCMP_CMP_GT" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::GE, "SCMP_CMP_GE" },
          { linyaps_box::Config::linux_t::seccomp_t::syscall_t::arg_t::op_t::MASKED_EQ,
            "SCMP_CMP_MASKED_EQ" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(seccomp_op_table));

constexpr auto seccomp_action_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::seccomp_t::action_t, 9>{
      { { { linyaps_box::Config::linux_t::seccomp_t::action_t::ALLOW, "SCMP_ACT_ALLOW" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::ERRNO, "SCMP_ACT_ERRNO" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::KILL, "SCMP_ACT_KILL" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::KILL_PROCESS,
            "SCMP_ACT_KILL_PROCESS" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::KILL_THREAD,
            "SCMP_ACT_KILL_THREAD" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::LOG, "SCMP_ACT_LOG" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::NOTIFY, "SCMP_ACT_NOTIFY" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::TRACE, "SCMP_ACT_TRACE" },
          { linyaps_box::Config::linux_t::seccomp_t::action_t::TRAP, "SCMP_ACT_TRAP" } } }
  };

// SCMP_ACT_KILL is aliased to KILL_THREAD or KILL_PROCESS depending on the
// libseccomp version — only (KILL, KILL_THREAD) or (KILL, KILL_PROCESS) may
// share the same value.
static_assert(linyaps_box::utils::verify_enum_table(
  seccomp_action_table, [](const auto &a, const auto &b) noexcept {
      if (a.name == b.name) {
          return false;
      }

      if (a.value == b.value) {
          return (a.name == "SCMP_ACT_KILL" && b.name == "SCMP_ACT_KILL_THREAD")
            || (a.name == "SCMP_ACT_KILL_THREAD" && b.name == "SCMP_ACT_KILL")
            || (a.name == "SCMP_ACT_KILL" && b.name == "SCMP_ACT_KILL_PROCESS")
            || (a.name == "SCMP_ACT_KILL_PROCESS" && b.name == "SCMP_ACT_KILL");
      }

      return true;
  }));

constexpr auto seccomp_flag_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::seccomp_t::flag_t, 4>{
      { { { linyaps_box::Config::linux_t::seccomp_t::flag_t::TSYNC, "SECCOMP_FILTER_FLAG_TSYNC" },
          { linyaps_box::Config::linux_t::seccomp_t::flag_t::LOG, "SECCOMP_FILTER_FLAG_LOG" },
          { linyaps_box::Config::linux_t::seccomp_t::flag_t::SPEC_ALLOW,
            "SECCOMP_FILTER_FLAG_SPEC_ALLOW" },
          { linyaps_box::Config::linux_t::seccomp_t::flag_t::WAIT_KILLABLE_RECV,
            "SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(seccomp_flag_table));
#endif

constexpr auto extra_flags_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::mount_t::extension, 2>{
      { { { linyaps_box::Config::mount_t::extension::COPY_SYMLINK, "copy-symlink" },
          { linyaps_box::Config::mount_t::extension::TMPCOPYUP, "tmpcopyup" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(extra_flags_table));

constexpr auto idmap_options_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::mount_t::idmap_type, 2>{
      { { { linyaps_box::Config::mount_t::idmap_type::IDMAP, "idmap" },
          { linyaps_box::Config::mount_t::idmap_type::RIDMAP, "ridmap" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(idmap_options_table));

auto parse_mount_options(const std::vector<std::string> &options)
  -> std::tuple<unsigned long,
                unsigned long,
                std::optional<linyaps_box::Config::mount_t::recursive_attr>,
                linyaps_box::Config::mount_t::extension,
                std::optional<linyaps_box::Config::mount_t::idmap_type>,
                std::string>
{
    using extension = linyaps_box::Config::mount_t::extension;

    auto vfs_flags{ 0UL };
    auto propagation_flags{ 0UL };
    std::optional<linyaps_box::Config::mount_t::recursive_attr> rec_attr;
    auto extension_flags{ extension::NONE };
    std::optional<linyaps_box::Config::mount_t::idmap_type> idmap;

    std::string data;

    for (const auto &opt : options) {
        if (const auto *entry = mo::find(mo::vfs, opt)) {
            vfs_flags |= entry->value;
            continue;
        }

        if (const auto *entry = mo::find(mo::unset, opt)) {
            vfs_flags &= ~entry->value;
            continue;
        }

        if (const auto *entry = mo::find(mo::propagation, opt)) {
            propagation_flags |= entry->value;
            continue;
        }

        if (const auto *entry = mo::find(mo::recursive_attr_set, opt)) {
            if (!rec_attr) {
                rec_attr.emplace();
            }
            rec_attr->set |= entry->value;
            continue;
        }

        if (const auto *entry = mo::find(mo::recursive_attr_clr, opt)) {
            if (!rec_attr) {
                rec_attr.emplace();
            }
            rec_attr->clr |= entry->value;
            continue;
        }

        if (auto ev = extra_flags_table.from_name(opt)) {
            extension_flags = extension_flags | *ev;
            continue;
        }

        // TODO: handle inline mapping strings like "idmap=uids=0:1000:1,gids=0:1000:1"
        if (auto iv = idmap_options_table.from_name(opt)) {
            if (UNLIKELY(idmap.has_value() && *idmap != *iv)) {
                throw std::runtime_error("idmap and ridmap options are mutually exclusive");
            }

            idmap.emplace(std::move(iv).value());
            continue;
        }

        if (!data.empty()) {
            data.push_back(',');
        }

        data.append(opt);
    }

    return { vfs_flags, propagation_flags, rec_attr, extension_flags, idmap, std::move(data) };
}

constexpr auto namespace_type_table =
  linyaps_box::utils::enum_table<linyaps_box::Config::linux_t::namespace_t::type, 8>{
      { { { linyaps_box::Config::linux_t::namespace_t::type::CGROUP, "cgroup" },
          { linyaps_box::Config::linux_t::namespace_t::type::IPC, "ipc" },
          { linyaps_box::Config::linux_t::namespace_t::type::MOUNT, "mount" },
          { linyaps_box::Config::linux_t::namespace_t::type::NET, "network" },
          { linyaps_box::Config::linux_t::namespace_t::type::PID, "pid" },
          { linyaps_box::Config::linux_t::namespace_t::type::TIME, "time" },
          { linyaps_box::Config::linux_t::namespace_t::type::USER, "user" },
          { linyaps_box::Config::linux_t::namespace_t::type::UTS, "uts" } } }
  };

static_assert(linyaps_box::utils::verify_enum_table(namespace_type_table));

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
using arch_t = linyaps_box::Config::linux_t::seccomp_t::arch_t;
constexpr auto seccomp_arch_table =
  linyaps_box::utils::enum_table<arch_t, 23>{ { { { arch_t::X86, "SCMP_ARCH_X86" },
                                                  { arch_t::X86_64, "SCMP_ARCH_X86_64" },
                                                  { arch_t::X32, "SCMP_ARCH_X32" },
                                                  { arch_t::ARM, "SCMP_ARCH_ARM" },
                                                  { arch_t::AARCH64, "SCMP_ARCH_AARCH64" },
                                                  { arch_t::MIPS, "SCMP_ARCH_MIPS" },
                                                  { arch_t::MIPS64, "SCMP_ARCH_MIPS64" },
                                                  { arch_t::MIPS64N32, "SCMP_ARCH_MIPS64N32" },
                                                  { arch_t::MIPSEL, "SCMP_ARCH_MIPSEL" },
                                                  { arch_t::MIPSEL64, "SCMP_ARCH_MIPSEL64" },
                                                  { arch_t::MIPSEL64N32, "SCMP_ARCH_MIPSEL64N32" },
                                                  { arch_t::PPC, "SCMP_ARCH_PPC" },
                                                  { arch_t::PPC64, "SCMP_ARCH_PPC64" },
                                                  { arch_t::PPC64LE, "SCMP_ARCH_PPC64LE" },
                                                  { arch_t::S390, "SCMP_ARCH_S390" },
                                                  { arch_t::S390X, "SCMP_ARCH_S390X" },
                                                  { arch_t::PARISC, "SCMP_ARCH_PARISC" },
                                                  { arch_t::PARISC64, "SCMP_ARCH_PARISC64" },
                                                  { arch_t::RISCV64, "SCMP_ARCH_RISCV64" },
                                                  { arch_t::LOONGARCH64, "SCMP_ARCH_LOONGARCH64" },
                                                  { arch_t::M68K, "SCMP_ARCH_M68K" },
                                                  { arch_t::SH, "SCMP_ARCH_SH" },
                                                  { arch_t::SHEB, "SCMP_ARCH_SHEB" } } } };
static_assert(linyaps_box::utils::verify_enum_table(seccomp_arch_table));
#endif

auto parse_range_list(std::string_view s) -> std::vector<unsigned int>
{
    std::vector<unsigned int> result;
    if (s.empty()) {
        return result;
    }

    result.reserve(16);

    const auto *ptr = s.data();
    const auto *end = ptr + s.size();

    auto skip_whitespace = [](const char *p, const char *e) {
        while (p < e && static_cast<unsigned char>(*p) <= ' ') {
            ++p;
        }

        return p;
    };

    while (ptr < end) {
        ptr = skip_whitespace(ptr, end);
        if (ptr == end) {
            break;
        }

        auto start{ 0U };
        auto [p_start, ec_start] = std::from_chars(ptr, end, start);
        if (UNLIKELY(ec_start != std::errc{ })) {
            if (ec_start == std::errc::result_out_of_range) {
                throw std::runtime_error(
                  "value overflow in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            throw std::runtime_error(
              "invalid value in range list at: "
              + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
        }

        ptr = p_start;
        ptr = skip_whitespace(ptr, end);

        if (ptr < end && *ptr == '-') {
            ++ptr;
            ptr = skip_whitespace(ptr, end);

            auto finish{ 0U };
            auto [p_finish, ec_finish] = std::from_chars(ptr, end, finish);
            if (UNLIKELY(ec_finish != std::errc{ })) {
                if (ec_finish == std::errc::result_out_of_range) {
                    throw std::runtime_error(
                      "value overflow in range list at: "
                      + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
                }
                throw std::runtime_error(
                  "invalid range end in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            ptr = p_finish;
            if (UNLIKELY(finish < start)) {
                throw std::runtime_error(
                  "invalid range in range list (finish < start) at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            if (UNLIKELY(start == 0 && finish == std::numeric_limits<unsigned int>::max())) {
                throw std::runtime_error(
                  "range too large in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            auto n = finish - start + 1;
            auto pos = result.size();
            result.resize(pos + n);
            for (unsigned int j = 0; j < n; ++j) {
                result[pos + j] = start + j;
            }
        } else {
            result.push_back(start);
        }

        ptr = skip_whitespace(ptr, end);
        if (ptr < end) {
            if (UNLIKELY(*ptr != ',')) {
                throw std::runtime_error(
                  "expected ',' or end-of-string in range list at: "
                  + std::string(ptr, std::min(end - ptr, static_cast<std::ptrdiff_t>(16))));
            }

            ++ptr;
        }
    }

    return result;
}

// see: https://pubs.opengroup.org/onlinepubs/9799919799/
auto is_invalid_env(std::string_view env) noexcept -> bool
{
    auto pos = env.find('=');
    if (pos == std::string_view::npos) {
        return true;
    }

    if (pos == 0) {
        return true;
    }

    if (env.find('\0') != std::string_view::npos) {
        return true;
    }

    return false;
}

} // namespace

namespace linyaps_box {

void from_json(const nlohmann::json &j, linyaps_box::Config::process_t::console_size_t &v)
{
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

void from_json(const nlohmann::json &j, linyaps_box::Config::process_t::rlimit_t &v)
{
    auto name = j.at("type").get<std::string_view>();
    auto opt = rlimit_type_table.from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error("unknown value: " + std::string(name));
    }
    v.type = *opt;
    j.at("soft").get_to(v.soft);
    j.at("hard").get_to(v.hard);
}

void from_json(const nlohmann::json &j, Config::process_t::user_t &v)
{
    j.at("uid").get_to(v.uid);
    j.at("gid").get_to(v.gid);

    if (auto it = j.find("umask"); it != j.end() && !it->is_null()) {
        it->get_to(v.umask.emplace());
    }

    if (auto it = j.find("additionalGids"); it != j.end() && !it->is_null()) {
        it->get_to(v.additional_gids.emplace());
    }
}

#ifdef LINYAPS_BOX_ENABLE_CAP
void from_json(const nlohmann::json &j, Config::process_t::capabilities_t &v)
{
    auto parse_set = [](const nlohmann::json &j, std::vector<cap_value_t> &set) {
        set.reserve(j.size());
        for (const auto &elem : j) {
            const auto &name = elem.get_ref<const std::string &>();
            cap_value_t val{ };
            if (UNLIKELY(cap_from_name(name.c_str(), &val) < 0)) {
                throw std::runtime_error("failed to parse cap " + name + ": " + ::strerror(errno));
            }

            set.push_back(val);
        }
    };

    if (auto it = j.find("effective"); it != j.end() && !it->is_null()) {
        parse_set(*it, v.effective.emplace());
    }

    if (auto it = j.find("ambient"); it != j.end() && !it->is_null()) {
        parse_set(*it, v.ambient.emplace());
    }

    if (auto it = j.find("bounding"); it != j.end() && !it->is_null()) {
        parse_set(*it, v.bounding.emplace());
    }

    if (auto it = j.find("inheritable"); it != j.end() && !it->is_null()) {
        parse_set(*it, v.inheritable.emplace());
    }

    if (auto it = j.find("permitted"); it != j.end() && !it->is_null()) {
        parse_set(*it, v.permitted.emplace());
    }
}

#endif // LINYAPS_BOX_ENABLE_CAP

void from_json(const nlohmann::json &j, Config::process_t::io_priority_t &v)
{
    auto name = j.at("class").get<std::string_view>();
    auto opt = io_priority_class_table.from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error("unknown value: " + std::string(name));
    }
    v.class_ = *opt;

    v.priority = j.value("priority", 0);

    if (UNLIKELY(v.priority < 0 || v.priority > 7)) {
        throw std::runtime_error("io priority must be in range [0, 7], got: "
                                 + std::to_string(v.priority));
    }
}

void from_json(const nlohmann::json &j, Config::process_t::scheduler_t &v)
{
    auto policy_name = j.at("policy").get<std::string_view>();
    auto policy_opt = scheduler_policy_table.from_name(policy_name);
    if (UNLIKELY(!policy_opt)) {
        throw std::runtime_error("unknown value: " + std::string(policy_name));
    }
    v.policy = *policy_opt;

    if (auto it = j.find("nice"); it != j.end() && !it->is_null()) {
        it->get_to(v.nice.emplace());
    }

    if (auto it = j.find("priority"); it != j.end() && !it->is_null()) {
        it->get_to(v.priority.emplace());
    }

    if (auto flags_it = j.find("flags"); flags_it != j.end() && !flags_it->is_null()) {
        Config::process_t::scheduler_t::flag_t flags{ };
        for (const auto &f : *flags_it) {
            const auto &flag_str = f.get_ref<const std::string &>();
            auto flag_opt = scheduler_flag_table.from_name(flag_str);
            if (UNLIKELY(!flag_opt)) {
                throw std::runtime_error("unknown value: " + std::string(flag_str));
            }
            flags = flags | *flag_opt;
        }

        v.flags = flags;
    }

    if (auto it = j.find("runtime"); it != j.end() && !it->is_null()) {
        it->get_to(v.runtime.emplace());
    }

    if (auto it = j.find("deadline"); it != j.end() && !it->is_null()) {
        it->get_to(v.deadline.emplace());
    }

    if (auto it = j.find("period"); it != j.end() && !it->is_null()) {
        it->get_to(v.period.emplace());
    }

    if (v.nice) {
        if (UNLIKELY(*v.nice < -20 || *v.nice > 19)) {
            throw std::runtime_error("scheduler.nice must be in range [-20, 19]");
        }
    }

    if (v.priority && *v.priority != 0) {
        if (v.policy != Config::process_t::scheduler_t::policy_t::FIFO
            && v.policy != Config::process_t::scheduler_t::policy_t::RR) {
            throw std::runtime_error(
              "scheduler.priority can only be specified for SCHED_FIFO or SCHED_RR");
        }
    }

    if (v.runtime || v.deadline || v.period) {
        if (v.policy != Config::process_t::scheduler_t::policy_t::DEADLINE) {
            throw std::runtime_error(
              "scheduler runtime/deadline/period can only be specified for SCHED_DEADLINE");
        }
    }
}

void from_json(const nlohmann::json &j, Config::process_t::exec_cpu_affinity_t &v)
{
    if (auto it = j.find("initial"); it != j.end() && !it->is_null()) {
        it->get_to(v.initial.emplace());
    }

    if (auto it = j.find("final"); it != j.end() && !it->is_null()) {
        it->get_to(v.final.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::process_t &v)
{
    if (auto it = j.find("terminal"); it != j.end() && !it->is_null()) {
        it->get_to(v.terminal.emplace());
    }

    if (v.terminal.value_or(false)) {
        if (auto it = j.find("consoleSize"); it != j.end() && !it->is_null()) {
            it->get_to(v.console_size.emplace());
        }
    }

    j.at("cwd").get_to(v.cwd);
    if (UNLIKELY(!v.cwd.is_absolute())) {
        throw std::runtime_error("process.cwd must be an absolute path, got: " + v.cwd.string());
    }

    if (auto it = j.find("env"); it != j.end() && !it->is_null()) {
        it->get_to(v.env.emplace());
        auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error("process.env contains a invalid env: " + *invalid);
        }
    }

    j.at("args").get_to(v.args);
    if (UNLIKELY(v.args.empty())) {
        throw std::runtime_error("process.args must not be empty");
    }

    if (auto it = j.find("rlimits"); it != j.end() && !it->is_null()) {
        it->get_to(v.rlimits.emplace());

        std::bitset<rlimit_type_table.entries.size()> seen;
        for (const auto &rl : *v.rlimits) {
            const auto type_val = static_cast<size_t>(rl.type);
            if (UNLIKELY(type_val >= seen.size())) {
                throw std::runtime_error("invalid rlimit type value out of range");
            }

            if (UNLIKELY(seen.test(type_val))) {
                throw std::runtime_error(
                  std::string{ "duplicate rlimit type: " }.append(to_string_view(rl.type)));
            }

            seen.set(type_val);
        }
    }

    if (auto it = j.find("apparmorProfile"); it != j.end() && !it->is_null()) {
        it->get_to(v.apparmor_profile.emplace());
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (auto it = j.find("capabilities"); it != j.end() && !it->is_null()) {
        it->get_to(v.capabilities.emplace());
    }
#endif

    if (auto it = j.find("noNewPrivileges"); it != j.end() && !it->is_null()) {
        it->get_to(v.no_new_privileges.emplace());
    }

    if (auto it = j.find("oomScoreAdj"); it != j.end() && !it->is_null()) {
        it->get_to(v.oom_score_adj.emplace());
    }

    if (auto it = j.find("scheduler"); it != j.end() && !it->is_null()) {
        it->get_to(v.scheduler.emplace());
    }

    if (auto it = j.find("selinuxLabel"); it != j.end() && !it->is_null()) {
        it->get_to(v.selinux_label.emplace());
    }

    if (auto it = j.find("ioPriority"); it != j.end() && !it->is_null()) {
        it->get_to(v.io_priority.emplace());
    }

    if (auto it = j.find("execCPUAffinity"); it != j.end() && !it->is_null()) {
        it->get_to(v.exec_cpu_affinity.emplace());
    }

    if (auto it = j.find("user"); it != j.end() && !it->is_null()) {
        it->get_to(v.user);
    } else {
        v.user = { };
    }
}

void from_json(const nlohmann::json &j, Config::id_mapping_t &v)
{
    j.at("hostID").get_to(v.host_id);
    j.at("containerID").get_to(v.container_id);
    j.at("size").get_to(v.size);
}

void from_json(const nlohmann::json &j, Config::linux_t::namespace_t &v)
{
    auto type_str = j.at("type").get<std::string_view>();
    auto opt = namespace_type_table.from_name(type_str);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error("unknown namespace type: " + std::string(type_str));
    }
    v.type_ = *opt;

    if (auto path_it = j.find("path"); path_it != j.end() && !path_it->is_null()) {
        auto path = path_it->get<std::string_view>();
        if (UNLIKELY(path.empty())) {
            throw std::runtime_error(
              std::string{ "namespace path must not be empty for type: " }.append(type_str));
        }

        auto &fs_path = v.path.emplace(path);
        if (UNLIKELY(!fs_path.is_absolute())) {
            throw std::runtime_error("namespace path must be absolute for type: "
                                     + std::string(type_str) + ", got: " + std::string(path));
        }
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::time_offset_t &v)
{
    j.at("secs").get_to(v.secs);
    j.at("nanosecs").get_to(v.nanosecs);
}

void from_json(const nlohmann::json &j, Config::linux_t::device_t &v)
{
    j.at("type").get_to(v.type);
    j.at("path").get_to(v.path);

    if (auto it = j.find("fileMode"); it != j.end() && !it->is_null()) {
        it->get_to(v.mode.emplace());
    }

    if (auto it = j.find("major"); it != j.end() && !it->is_null()) {
        it->get_to(v.major.emplace());
    }

    if (auto it = j.find("minor"); it != j.end() && !it->is_null()) {
        it->get_to(v.minor.emplace());
    }

    if (auto it = j.find("uid"); it != j.end() && !it->is_null()) {
        it->get_to(v.uid.emplace());
    }

    if (auto it = j.find("gid"); it != j.end() && !it->is_null()) {
        it->get_to(v.gid.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::network_device_t &v)
{
    if (auto it = j.find("name"); it != j.end() && !it->is_null()) {
        it->get_to(v.name.emplace());
    }
}

// --- allowed_device_t ---

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::device_t &v)
{
    j.at("allow").get_to(v.allow);
    if (auto it = j.find("type"); it != j.end() && !it->is_null()) {
        it->get_to(v.type.emplace());
    }

    if (auto it = j.find("major"); it != j.end() && !it->is_null()) {
        it->get_to(v.major.emplace());
    }

    if (auto it = j.find("minor"); it != j.end() && !it->is_null()) {
        it->get_to(v.minor.emplace());
    }

    if (auto it = j.find("access"); it != j.end() && !it->is_null()) {
        it->get_to(v.access.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::memory_t &v)
{
    if (auto it = j.find("limit"); it != j.end() && !it->is_null()) {
        it->get_to(v.limit.emplace());
    }

    if (auto it = j.find("reservation"); it != j.end() && !it->is_null()) {
        it->get_to(v.reservation.emplace());
    }
    if (auto it = j.find("swap"); it != j.end() && !it->is_null()) {
        it->get_to(v.swap.emplace());
    }

    if (auto it = j.find("kernel"); it != j.end() && !it->is_null()) {
        it->get_to(v.kernel.emplace());
    }

    if (auto it = j.find("kernelTCP"); it != j.end() && !it->is_null()) {
        it->get_to(v.kernel_tcp.emplace());
    }

    if (auto it = j.find("swappiness"); it != j.end() && !it->is_null()) {
        it->get_to(v.swappiness.emplace());
    }

    if (auto it = j.find("disableOOMKiller"); it != j.end() && !it->is_null()) {
        it->get_to(v.disable_OOM_killer.emplace());
    }

    if (auto it = j.find("useHierarchy"); it != j.end() && !it->is_null()) {
        it->get_to(v.use_hierarchy.emplace());
    }

    if (auto it = j.find("checkBeforeUpdate"); it != j.end() && !it->is_null()) {
        it->get_to(v.check_before_update.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::cpu_t &v)
{
    if (auto it = j.find("shares"); it != j.end() && !it->is_null()) {
        it->get_to(v.shares.emplace());
    }

    if (auto it = j.find("quota"); it != j.end() && !it->is_null()) {
        it->get_to(v.quota.emplace());
    }

    if (auto it = j.find("burst"); it != j.end() && !it->is_null()) {
        it->get_to(v.burst.emplace());
    }

    if (auto it = j.find("period"); it != j.end() && !it->is_null()) {
        it->get_to(v.period.emplace());
    }

    if (auto it = j.find("realtimeRuntime"); it != j.end() && !it->is_null()) {
        it->get_to(v.realtime_runtime.emplace());
    }
    if (auto it = j.find("realtimePeriod"); it != j.end() && !it->is_null()) {
        it->get_to(v.realtime_period.emplace());
    }

    if (auto it = j.find("cpus"); it != j.end() && !it->is_null()) {
        v.cpus = parse_range_list(it->get_ref<const std::string &>());
    }

    if (auto it = j.find("mems"); it != j.end() && !it->is_null()) {
        v.mems = parse_range_list(it->get_ref<const std::string &>());
    }

    if (auto it = j.find("idle"); it != j.end() && !it->is_null()) {
        // TODO: move this logic to converter?
        auto val = it->get<int64_t>();
        v.idle.emplace((val == 1) ? Config::linux_t::resources_t::cpu_t::idle_t::IDLE
                                  : Config::linux_t::resources_t::cpu_t::idle_t::NONE);
    }
}

void from_json(const nlohmann::json &j,
               Config::linux_t::resources_t::block_io_t::weight_device_t &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);

    if (auto it = j.find("weight"); it != j.end() && !it->is_null()) {
        it->get_to(v.weight.emplace());
    }

    if (auto it = j.find("leafWeight"); it != j.end() && !it->is_null()) {
        it->get_to(v.leaf_weight.emplace());
    }
}

void from_json(const nlohmann::json &j,
               Config::linux_t::resources_t::block_io_t::throttle_device_t &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);
    j.at("rate").get_to(v.rate);
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::block_io_t &v)
{
    if (auto it = j.find("weight"); it != j.end() && !it->is_null()) {
        it->get_to(v.weight.emplace());
    }

    if (auto it = j.find("leafWeight"); it != j.end() && !it->is_null()) {
        it->get_to(v.leaf_weight.emplace());
    }

    if (auto it = j.find("weightDevice"); it != j.end() && !it->is_null()) {
        it->get_to(v.weight_devices.emplace());
    }

    if (auto it = j.find("throttleReadBpsDevice"); it != j.end() && !it->is_null()) {
        it->get_to(v.throttle_read_bps_device.emplace());
    }

    if (auto it = j.find("throttleWriteBpsDevice"); it != j.end() && !it->is_null()) {
        it->get_to(v.throttle_write_bps_device.emplace());
    }

    if (auto it = j.find("throttleReadIOPSDevice"); it != j.end() && !it->is_null()) {
        it->get_to(v.throttle_read_iops_device.emplace());
    }

    if (auto it = j.find("throttleWriteIOPSDevice"); it != j.end() && !it->is_null()) {
        it->get_to(v.throttle_write_iops_device.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::hugepage_limit_t &v)
{
    j.at("pageSize").get_to(v.page_size);
    j.at("limit").get_to(v.limit);
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::network_t::priority_t &v)
{
    j.at("name").get_to(v.name);
    j.at("priority").get_to(v.priority);
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::network_t &v)
{
    if (auto it = j.find("classID"); it != j.end() && !it->is_null()) {
        it->get_to(v.class_id.emplace());
    }

    if (auto it = j.find("priorities"); it != j.end() && !it->is_null()) {
        it->get_to(v.priorities.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::pids_t &v)
{
    if (auto it = j.find("limit"); it != j.end() && !it->is_null()) {
        it->get_to(v.limit.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t::rdma_t &v)
{
    if (auto it = j.find("hcaHandles"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_handles.emplace());
    }

    if (auto it = j.find("hcaObjects"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_objects.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::intel_rdt_t &v)
{
    if (auto it = j.find("closID"); it != j.end() && !it->is_null()) {
        it->get_to(v.clos_id.emplace());
    }

    if (auto it = j.find("l3CacheSchema"); it != j.end() && !it->is_null()) {
        it->get_to(v.l3_cache_schema.emplace());
    }

    if (auto it = j.find("memBwSchema"); it != j.end() && !it->is_null()) {
        it->get_to(v.memory_bandwidth_schema.emplace());
    }

    if (auto it = j.find("schemata"); it != j.end() && !it->is_null()) {
        it->get_to(v.schemata.emplace());
    }

    if (auto it = j.find("enableMonitoring"); it != j.end() && !it->is_null()) {
        it->get_to(v.enable_monitoring.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::resources_t &v)
{
    if (auto it = j.find("unified"); it != j.end() && !it->is_null()) {
        it->get_to(v.unified.emplace());
    }

    if (auto it = j.find("devices"); it != j.end() && !it->is_null()) {
        it->get_to(v.devices.emplace());
    }

    if (auto it = j.find("pids"); it != j.end() && !it->is_null()) {
        it->get_to(v.pids.emplace());
    }

    if (auto it = j.find("memory"); it != j.end() && !it->is_null()) {
        it->get_to(v.memory.emplace());
    }

    if (auto it = j.find("cpu"); it != j.end() && !it->is_null()) {
        it->get_to(v.cpu.emplace());
    }

    if (auto it = j.find("hugepageLimits"); it != j.end() && !it->is_null()) {
        it->get_to(v.hugepage_limits.emplace());
    }

    if (auto it = j.find("blockIO"); it != j.end() && !it->is_null()) {
        it->get_to(v.block_io.emplace());
    }

    if (auto it = j.find("network"); it != j.end() && !it->is_null()) {
        it->get_to(v.network.emplace());
    }

    if (auto it = j.find("rdma"); it != j.end() && !it->is_null()) {
        it->get_to(v.rdma.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::memory_policy_t &v)
{
    auto mode_name = j.at("mode").get<std::string_view>();
    auto mode_opt = memory_policy_mode_table.from_name(mode_name);
    if (UNLIKELY(!mode_opt)) {
        throw std::runtime_error("unknown value: " + std::string(mode_name));
    }
    v.mode = *mode_opt;

    if (auto nodes_it = j.find("nodes"); nodes_it != j.end() && !nodes_it->is_null()) {
        v.nodes = parse_range_list(nodes_it->get<std::string_view>());
    }

    if (auto flags_it = j.find("flags"); flags_it != j.end() && !flags_it->is_null()) {
        Config::linux_t::memory_policy_t::flag_t flags{ };
        for (const auto &f : *flags_it) {
            const auto &flag_str = f.get_ref<const std::string &>();
            auto flag_opt = memory_policy_flag_table.from_name(flag_str);
            if (UNLIKELY(!flag_opt)) {
                throw std::runtime_error("unknown value: " + std::string(flag_str));
            }
            flags = flags | *flag_opt;
        }

        v.flags = flags;
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::personality_t &v)
{
    auto domain_name = j.at("domain").get<std::string_view>();
    auto domain_opt = personality_domain_table.from_name(domain_name);
    if (UNLIKELY(!domain_opt)) {
        throw std::runtime_error("unknown value: " + std::string(domain_name));
    }
    v.domain = *domain_opt;

    if (auto flags_it = j.find("flags"); flags_it != j.end() && !flags_it->is_null()) {
        flags_it->get_to(v.flags.emplace());
    }
}

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
void from_json(const nlohmann::json &j, Config::linux_t::seccomp_t::syscall_t::arg_t &v)
{
    j.at("index").get_to(v.index);
    j.at("value").get_to(v.value);

    if (auto value_two_it = j.find("valueTwo");
        value_two_it != j.end() && !value_two_it->is_null()) {
        value_two_it->get_to(v.value_two.emplace());
    }

    auto op_name = j.at("op").get<std::string_view>();
    auto op_opt = seccomp_op_table.from_name(op_name);
    if (UNLIKELY(!op_opt)) {
        throw std::runtime_error("unknown value: " + std::string(op_name));
    }
    v.op = *op_opt;
}

void from_json(const nlohmann::json &j, Config::linux_t::seccomp_t::syscall_t &v)
{
    j.at("names").get_to(v.names);
    if (UNLIKELY(v.names.empty())) {
        throw std::runtime_error("seccomp syscall names must not be empty");
    }

    using action_t = Config::linux_t::seccomp_t::action_t;
    {
        auto action_name = j.at("action").get<std::string_view>();
        auto action_opt = seccomp_action_table.from_name(action_name);
        if (UNLIKELY(!action_opt)) {
            throw std::runtime_error("unknown value: " + std::string(action_name));
        }
        v.action = *action_opt;
    }

    if (auto it = j.find("errnoRet"); it != j.end() && !it->is_null()) {
        it->get_to(v.errno_ret.emplace());
        if (UNLIKELY(v.action != action_t::ERRNO && v.action != action_t::TRACE)) {
            throw std::runtime_error(
              "seccomp syscall errnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
        }
    }

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::linux_t::seccomp_t &v)
{
    using action_t = Config::linux_t::seccomp_t::action_t;
    {
        auto action_name = j.at("defaultAction").get<std::string_view>();
        auto action_opt = seccomp_action_table.from_name(action_name);
        if (UNLIKELY(!action_opt)) {
            throw std::runtime_error("unknown value: " + std::string(action_name));
        }
        v.default_action = *action_opt;
    }

    if (auto it = j.find("defaultErrnoRet"); it != j.end() && !it->is_null()) {
        it->get_to(v.default_errno_ret.emplace());
    }

    if (UNLIKELY(v.default_errno_ret && v.default_action != action_t::ERRNO
                 && v.default_action != action_t::TRACE)) {
        throw std::runtime_error(
          "seccomp defaultErrnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    }

    if (auto it = j.find("architectures"); it != j.end() && !it->is_null()) {
        std::vector<Config::linux_t::seccomp_t::arch_t> archs;
        archs.reserve(it->size());
        for (const auto &elem : *it) {
            auto arch_str = elem.get<std::string_view>();
            auto arch_opt = seccomp_arch_table.from_name(arch_str);
            if (UNLIKELY(!arch_opt)) {
                throw std::runtime_error("unknown architecture: " + std::string(arch_str));
            }
            archs.push_back(*arch_opt);
        }

        v.architectures = std::move(archs);
    }

    if (auto it = j.find("flags"); it != j.end() && !it->is_null()) {
        Config::linux_t::seccomp_t::flag_t flags{ };
        for (const auto &elem : *it) {
            auto flag_str = elem.get<std::string_view>();
            auto flag_opt = seccomp_flag_table.from_name(flag_str);
            if (UNLIKELY(!flag_opt)) {
                throw std::runtime_error("unknown value: " + std::string(flag_str));
            }

            flags = flags | *flag_opt;
        }

        v.flags = flags;
    }

    if (auto it = j.find("listenerPath"); it != j.end() && !it->is_null()) {
        it->get_to(v.listener_path.emplace());
    }

    if (auto it = j.find("listenerMetadata"); it != j.end() && !it->is_null()) {
        it->get_to(v.listener_metadata.emplace());
    }

    if (UNLIKELY(v.default_action == action_t::NOTIFY && !v.listener_path)) {
        throw std::runtime_error("seccomp SCMP_ACT_NOTIFY requires listenerPath");
    }

    if (auto it = j.find("syscalls"); it != j.end() && !it->is_null()) {
        it->get_to(v.syscalls.emplace());
    }

    if (UNLIKELY(v.listener_metadata && !v.listener_path)) {
        throw std::runtime_error("seccomp listenerMetadata requires listenerPath to be set");
    }
}

#endif

void from_json(const nlohmann::json &j, Config::linux_t &v)
{
    if (auto it = j.find("uidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.uid_mappings.emplace());
    }

    if (auto it = j.find("gidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.gid_mappings.emplace());
    }

    if (auto it = j.find("namespaces"); it != j.end() && !it->is_null()) {
        it->get_to(v.namespaces.emplace());

        auto seen{ 0U };
        for (const auto &ns : *v.namespaces) {
            // no need to use std::bitset
            auto val = static_cast<unsigned int>(ns.type_);
            if (UNLIKELY((seen & val) != 0)) {
                throw std::runtime_error(
                  std::string{ namespace_type_table.to_name(ns.type_).value_or("unknown") }
                  + ": duplicate namespace type");
            }

            seen |= val;
        }
    }

    if (auto it = j.find("devices"); it != j.end() && !it->is_null()) {
        it->get_to(v.devices.emplace());
    }

    if (auto it = j.find("netDevices"); it != j.end() && !it->is_null()) {
        it->get_to(v.network_devices.emplace());
    }

    if (auto it = j.find("cgroupsPath"); it != j.end() && !it->is_null()) {
        it->get_to(v.cgroups_path.emplace());
    }

    if (auto it = j.find("maskedPaths"); it != j.end() && !it->is_null()) {
        it->get_to(v.masked_paths.emplace());
    }

    if (auto it = j.find("readonlyPaths"); it != j.end() && !it->is_null()) {
        it->get_to(v.readonly_paths.emplace());
    }

    if (auto it = j.find("mountLabel"); it != j.end() && !it->is_null()) {
        it->get_to(v.mount_label.emplace());
    }

    if (auto it = j.find("rootfsPropagation"); it != j.end() && !it->is_null()) {
        auto prop_name = it->get<std::string_view>();

        struct
        {
            const char *name;
            unsigned long value;
        } const table[]{
            { "private", MS_PRIVATE },       { "rprivate", MS_PRIVATE | MS_REC },
            { "shared", MS_SHARED },         { "rshared", MS_SHARED | MS_REC },
            { "slave", MS_SLAVE },           { "rslave", MS_SLAVE | MS_REC },
            { "unbindable", MS_UNBINDABLE }, { "runbindable", MS_UNBINDABLE | MS_REC },
        };

        bool found = false;
        for (const auto &entry : table) {
            if (prop_name == entry.name) {
                v.rootfs_propagation = entry.value;
                found = true;
                break;
            }
        }

        if (UNLIKELY(!found)) {
            throw std::runtime_error("unknown value: " + std::string(prop_name));
        }
    } else {
        v.rootfs_propagation = MS_PRIVATE | MS_REC;
    }

    if (auto it = j.find("sysctl"); it != j.end() && !it->is_null()) {
        it->get_to(v.sysctl.emplace());
    }

    if (auto it = j.find("timeOffsets"); it != j.end() && !it->is_null()) {
        auto &offsets = v.time_offsets.emplace();
        for (const auto &[clock_name, offset_json] : it->items()) {
            auto [it, ignored] =
              offsets.try_emplace(clock_name, offset_json.get<Config::linux_t::time_offset_t>());
            if (UNLIKELY(!ignored)) {
                throw std::runtime_error("duplicated timeOffset :" + offset_json.dump());
            }
        }
    }

    if (auto it = j.find("personality"); it != j.end() && !it->is_null()) {
        it->get_to(v.personality.emplace());
    }

    if (auto it = j.find("memoryPolicy"); it != j.end() && !it->is_null()) {
        it->get_to(v.memory_policy.emplace());
    }

    if (auto it = j.find("intelRdt"); it != j.end() && !it->is_null()) {
        it->get_to(v.intel_rdt.emplace());
    }

    if (auto it = j.find("resources"); it != j.end() && !it->is_null()) {
        it->get_to(v.resources.emplace());
    }

#ifdef LINYAPS_BOX_ENABLE_SECCOMP
    if (auto it = j.find("seccomp"); it != j.end() && !it->is_null()) {
        it->get_to(v.seccomp.emplace());
    }
#endif
}

void from_json(const nlohmann::json &j, Config::hooks_t::hook_t &v)
{
    j.at("path").get_to(v.path);
    if (UNLIKELY(!v.path.is_absolute())) {
        throw std::runtime_error("hook path must be absolute");
    }

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }

    if (auto it = j.find("env"); it != j.end() && !it->is_null()) {
        const auto &env = it->get_to(v.env.emplace());
        auto invalid = std::find_if(env.cbegin(), env.cend(), is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error("hook.env contains a invalid env: " + *invalid);
        }
    }

    if (auto it = j.find("timeout"); it != j.end() && !it->is_null()) {
        it->get_to(v.timeout.emplace());
        if (UNLIKELY(v.timeout.value() <= 0)) {
            throw std::runtime_error("hook timeout must be greater than zero");
        }
    }
}

void from_json(const nlohmann::json &j, Config::hooks_t &v)
{
    if (auto it = j.find("prestart"); it != j.end() && !it->is_null()) {
        it->get_to(v.prestart.emplace());
    }

    if (auto it = j.find("createRuntime"); it != j.end() && !it->is_null()) {
        it->get_to(v.create_runtime.emplace());
    }

    if (auto it = j.find("createContainer"); it != j.end() && !it->is_null()) {
        it->get_to(v.create_container.emplace());
    }

    if (auto it = j.find("startContainer"); it != j.end() && !it->is_null()) {
        it->get_to(v.start_container.emplace());
    }

    if (auto it = j.find("poststart"); it != j.end() && !it->is_null()) {
        it->get_to(v.poststart.emplace());
    }

    if (auto it = j.find("poststop"); it != j.end() && !it->is_null()) {
        it->get_to(v.poststop.emplace());
    }
}

void from_json(const nlohmann::json &j, Config::mount_t &v)
{
    j.at("destination").get_to(v.destination);

    if (auto it = j.find("source"); it != j.end() && !it->is_null()) {
        it->get_to(v.source.emplace());
    }
    if (auto it = j.find("type"); it != j.end() && !it->is_null()) {
        it->get_to(v.type.emplace());
    }
    if (auto it = j.find("uidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.uid_mappings.emplace());
    }
    if (auto it = j.find("gidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.gid_mappings.emplace());
    }

    if (auto it = j.find("options"); it != j.end() && !it->is_null()) {
        auto options = it->get<std::vector<std::string>>();
        std::tie(v.vfs_flags, v.propagation_flags, v.rec_attr, v.extension_flags, v.idmap, v.data) =
          parse_mount_options(options);
    }
}

void from_json(const nlohmann::json &j, Config::root_t &v)
{
    j.at("path").get_to(v.path);
    v.readonly = j.value("readonly", false);
}

void from_json(const nlohmann::json &j, Config &v)
{
    auto semver = linyaps_box::utils::semver(j.at("ociVersion").get_ref<const std::string &>());
    if (UNLIKELY(!linyaps_box::utils::semver(Config::oci_version).is_compatible_with(semver))) {
        throw std::runtime_error("unsupported OCI version: " + semver.to_string());
    }

    if (auto it = j.find("process"); it != j.end() && !it->is_null()) {
        it->get_to(v.process.emplace());
    }

    if (auto it = j.find("hostname"); it != j.end() && !it->is_null()) {
        it->get_to(v.hostname.emplace());
    }

    if (auto it = j.find("domainname"); it != j.end() && !it->is_null()) {
        it->get_to(v.domainname.emplace());
    }

    if (auto it = j.find("linux"); it != j.end() && !it->is_null()) {
        it->get_to(v.linux.emplace());
    }

    if (auto it = j.find("hooks"); it != j.end() && !it->is_null()) {
        it->get_to(v.hooks.emplace());
    }

    if (auto it = j.find("mounts"); it != j.end() && !it->is_null()) {
        it->get_to(v.mounts);
    }

    if (auto it = j.find("root"); it != j.end() && !it->is_null()) {
        it->get_to(v.root.emplace());
    }

    if (auto it = j.find("annotations"); it != j.end() && !it->is_null()) {
        it->get_to(v.annotations.emplace());
    }
}

auto to_string_view(Config::linux_t::namespace_t::type type) noexcept -> std::string_view
{
    return namespace_type_table.to_name(type).value_or(std::string_view{ });
}

auto to_string_view(linyaps_box::Config::process_t::rlimit_t::type_t type) noexcept
  -> std::string_view
{
    return rlimit_type_table.to_name(type).value_or(std::string_view{ });
}

auto validate_namespace_path(const Config::linux_t::namespace_t &ns) -> void
{
    if (!ns.path) {
        return;
    }

    auto fs_path = ns.path.value();
    std::error_code ec;
    auto target = std::filesystem::read_symlink(fs_path, ec);
    if (UNLIKELY(!!ec)) {
        throw std::runtime_error("namespace path " + fs_path.string()
                                 + " does not exist or is not accessible");
    }

    auto target_str = target.string();
    auto target_view = std::string_view{ target_str };

    auto colon_pos = target_view.find(':');
    if (UNLIKELY(colon_pos == std::string_view::npos)) {
        throw std::runtime_error("namespace path " + fs_path.string()
                                 + " does not appear to be a namespace file");
    }

    auto ns_type_from_path = target_view.substr(0, colon_pos);
    auto expected_str = namespace_type_table.to_name(ns.type_).value();
    if (UNLIKELY(ns_type_from_path != expected_str)) {
        throw std::runtime_error("namespace path " + fs_path.string() + " is associated with '"
                                 + std::string(ns_type_from_path) + "' namespace, not '"
                                 + std::string(expected_str) + "'");
    }
}

auto Config::parse(std::string_view content) -> Config
{
    return nlohmann::json::parse(content).get<Config>();
}

auto Config::parse(const std::filesystem::path &path) -> Config
{
    std::ifstream stream{ path, std::ios::binary | std::ios::in };
    if (UNLIKELY(!stream.is_open())) {
        throw std::filesystem::filesystem_error(
          "failed to open config file",
          path,
          std::make_error_code(static_cast<std::errc>(errno)));
    }

    return nlohmann::json::parse(stream).get<Config>();
}

} // namespace linyaps_box
