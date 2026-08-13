// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config.h"

#include "linyaps_box/config/enum_tables.h"
#include "linyaps_box/config/mount_options.h"
#include "linyaps_box/config/validate.h"
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/semver.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <charconv>
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

// Convenience aliases for tables defined in enum_tables.h
constexpr auto rlimit_type_table =
  get_enum_table(static_cast<linyaps_box::oci_config::process_t::rlimit_t::type_t *>(nullptr));
constexpr auto scheduler_policy_table =
  get_enum_table(static_cast<linyaps_box::oci_config::process_t::scheduler_t::policy_t *>(nullptr));
constexpr auto scheduler_flag_table =
  get_enum_table(static_cast<linyaps_box::oci_config::process_t::scheduler_t::flag_t *>(nullptr));
constexpr auto io_priority_class_table = get_enum_table(
  static_cast<linyaps_box::oci_config::process_t::io_priority_t::class_t *>(nullptr));
constexpr auto personality_domain_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::personality_t::domain_t *>(nullptr));
constexpr auto memory_policy_mode_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::memory_policy_t::mode_t *>(nullptr));
constexpr auto memory_policy_flag_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::memory_policy_t::flag_t *>(nullptr));
constexpr auto seccomp_op_table = get_enum_table(
  static_cast<linyaps_box::oci_config::linux_t::seccomp_t::syscall_t::arg_t::op_t *>(nullptr));
constexpr auto seccomp_action_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::seccomp_t::action_t *>(nullptr));
constexpr auto seccomp_flag_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::seccomp_t::flag_t *>(nullptr));
constexpr auto extra_flags_table =
  get_enum_table(static_cast<linyaps_box::oci_config::mount_t::extension *>(nullptr));
constexpr auto idmap_options_table =
  get_enum_table(static_cast<linyaps_box::oci_config::mount_t::idmap_type *>(nullptr));
constexpr auto namespace_type_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::namespace_t::type *>(nullptr));
constexpr auto seccomp_arch_table =
  get_enum_table(static_cast<linyaps_box::oci_config::linux_t::seccomp_t::arch_t *>(nullptr));

namespace {

namespace mo = linyaps_box::config::mount_options;

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

auto parse_id_mapping_chunk(std::string_view chunk) -> linyaps_box::oci_config::id_mapping_t
{
    // format: "containerID:hostID:size"
    auto first_colon = chunk.find(':');
    if (UNLIKELY(first_colon == std::string_view::npos)) {
        throw std::runtime_error("invalid id mapping: " + std::string(chunk));
    }

    auto second_colon = chunk.find(':', first_colon + 1);
    if (UNLIKELY(second_colon == std::string_view::npos)) {
        throw std::runtime_error("invalid id mapping: " + std::string(chunk));
    }

    linyaps_box::oci_config::id_mapping_t mapping{ };
    auto [p1, ec1] =
      std::from_chars(chunk.cbegin(), chunk.cbegin() + first_colon, mapping.container_id);
    if (UNLIKELY(ec1 != std::errc{ })) {
        throw std::runtime_error("invalid container id in mapping: " + std::string(chunk));
    }

    auto [p2, ec2] =
      std::from_chars(chunk.data() + first_colon + 1, chunk.data() + second_colon, mapping.host_id);
    if (UNLIKELY(ec2 != std::errc{ })) {
        throw std::runtime_error("invalid host id in mapping: " + std::string(chunk));
    }

    auto [p3, ec3] =
      std::from_chars(chunk.data() + second_colon + 1, chunk.data() + chunk.size(), mapping.size);
    if (UNLIKELY(ec3 != std::errc{ })) {
        throw std::runtime_error("invalid size in mapping: " + std::string(chunk));
    }

    return mapping;
};

auto parse_mappings(std::string_view value)
  -> std::optional<std::vector<linyaps_box::oci_config::id_mapping_t>>
{
    std::vector<linyaps_box::oci_config::id_mapping_t> result;

    while (!value.empty()) {
        auto comma_pos = value.find(',');
        auto chunk = value.substr(0, comma_pos);
        result.push_back(parse_id_mapping_chunk(chunk));

        if (comma_pos == std::string_view::npos) {
            break;
        }

        value = value.substr(comma_pos + 1);
    }

    return result;
}

struct inline_idmap_result
{
    linyaps_box::oci_config::mount_t::idmap_type type;
    std::optional<std::vector<linyaps_box::oci_config::id_mapping_t>> uid_mappings;
    std::optional<std::vector<linyaps_box::oci_config::id_mapping_t>> gid_mappings;
};

auto parse_inline_idmap_option(std::string_view opt) -> inline_idmap_result
{
    auto eq_pos = opt.find('=');
    auto prefix = opt.substr(0, eq_pos);
    auto iv = idmap_options_table.from_name(prefix);
    if (UNLIKELY(!iv)) {
        throw std::runtime_error("unknown idmap option: " + std::string(opt));
    }

    inline_idmap_result result;
    result.type = *iv;
    auto rest = opt.substr(eq_pos + 1);

    while (!rest.empty()) {
        auto comma_pos = rest.find(',');
        auto part = rest.substr(0, comma_pos);

        auto eq2_pos = part.find('=');
        if (eq2_pos == std::string_view::npos) {
            throw std::runtime_error("invalid id mapping option: " + std::string(opt));
        }

        auto key = part.substr(0, eq2_pos);
        auto value = part.substr(eq2_pos + 1);

        if (key == "uids") {
            result.uid_mappings = parse_mappings(value);
        } else if (key == "gids") {
            result.gid_mappings = parse_mappings(value);
        } else {
            throw std::runtime_error("unknown id mapping key: " + std::string(key));
        }

        if (comma_pos == std::string_view::npos) {
            break;
        }

        rest = rest.substr(comma_pos + 1);
    }

    return result;
}

} // namespace

auto parse_mount_options(const std::vector<std::string> &options)
  -> std::tuple<unsigned long,
                unsigned long,
                std::optional<linyaps_box::oci_config::mount_t::recursive_attr>,
                linyaps_box::oci_config::mount_t::extension,
                std::optional<linyaps_box::oci_config::mount_t::idmap_type>,
                std::optional<std::vector<linyaps_box::oci_config::id_mapping_t>>,
                std::optional<std::vector<linyaps_box::oci_config::id_mapping_t>>,
                std::string>
{
    using extension = linyaps_box::oci_config::mount_t::extension;
    using id_mapping_t = linyaps_box::oci_config::id_mapping_t;

    auto vfs_flags{ 0UL };
    auto propagation_flags{ 0UL };
    auto extension_flags{ extension::NONE };
    std::optional<linyaps_box::oci_config::mount_t::recursive_attr> rec_attr;
    std::optional<linyaps_box::oci_config::mount_t::idmap_type> idmap;
    std::optional<std::vector<id_mapping_t>> uid_mappings;
    std::optional<std::vector<id_mapping_t>> gid_mappings;
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

        // Handle simple "idmap" or "ridmap" flags
        if (auto iv = idmap_options_table.from_name(opt)) {
            if (UNLIKELY(idmap.has_value() && *idmap != *iv)) {
                throw std::runtime_error("idmap and ridmap options are mutually exclusive");
            }

            idmap.emplace(std::move(iv).value());
            continue;
        }

        // Handle inline mapping strings like "idmap=uids=0:1000:1,gids=0:1000:1"
        if (auto eq_pos = opt.find('='); UNLIKELY(eq_pos != std::string_view::npos)) {
            auto prefix = std::string_view{ opt }.substr(0, eq_pos);
            if (auto iv = idmap_options_table.from_name(prefix)) {
                if (UNLIKELY(idmap.has_value() && *idmap != *iv)) {
                    throw std::runtime_error("idmap and ridmap options are mutually exclusive");
                }

                auto inline_result = parse_inline_idmap_option(opt);
                idmap.emplace(inline_result.type);
                uid_mappings = std::move(inline_result.uid_mappings);
                gid_mappings = std::move(inline_result.gid_mappings);

                continue;
            }
        }

        if (!data.empty()) {
            data.push_back(',');
        }

        data.append(opt);
    }

    return { vfs_flags,
             propagation_flags,
             rec_attr,
             extension_flags,
             idmap,
             std::move(uid_mappings),
             std::move(gid_mappings),
             std::move(data) };
}

namespace linyaps_box {

void from_json(const nlohmann::json &j, linyaps_box::oci_config::process_t::console_size_t &v)
{
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

void from_json(const nlohmann::json &j, linyaps_box::oci_config::process_t::rlimit_t &v)
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

void from_json(const nlohmann::json &j, oci_config::process_t::user_t &v)
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

void from_json(const nlohmann::json &j, oci_config::process_t::capabilities_t &v)
{
    auto parse_set = [](const nlohmann::json &j, std::vector<std::string> &set) {
        set.reserve(j.size());
        for (const auto &elem : j) {
            set.push_back(elem.get<std::string>());
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

void from_json(const nlohmann::json &j, oci_config::process_t::io_priority_t &v)
{
    auto name = j.at("class").get<std::string_view>();
    auto opt = io_priority_class_table.from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error("unknown value: " + std::string(name));
    }
    v.class_ = *opt;

    v.priority = j.value("priority", 0);
}

void from_json(const nlohmann::json &j, oci_config::process_t::scheduler_t &v)
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
        oci_config::process_t::scheduler_t::flag_t flags{ };
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
}

void from_json(const nlohmann::json &j, oci_config::process_t::exec_cpu_affinity_t &v)
{
    if (auto it = j.find("initial"); it != j.end() && !it->is_null()) {
        it->get_to(v.initial.emplace());
    }

    if (auto it = j.find("final"); it != j.end() && !it->is_null()) {
        it->get_to(v.final.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::process_t &v)
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

    if (auto it = j.find("env"); it != j.end() && !it->is_null()) {
        it->get_to(v.env.emplace());
    }

    j.at("args").get_to(v.args);

    if (auto it = j.find("rlimits"); it != j.end() && !it->is_null()) {
        it->get_to(v.rlimits.emplace());
    }

    if (auto it = j.find("apparmorProfile"); it != j.end() && !it->is_null()) {
        it->get_to(v.apparmor_profile.emplace());
    }

    if (auto it = j.find("capabilities"); it != j.end() && !it->is_null()) {
        it->get_to(v.capabilities.emplace());
    }

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

void from_json(const nlohmann::json &j, oci_config::id_mapping_t &v)
{
    j.at("hostID").get_to(v.host_id);
    j.at("containerID").get_to(v.container_id);
    j.at("size").get_to(v.size);
}

void from_json(const nlohmann::json &j, oci_config::linux_t::namespace_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::time_offset_t &v)
{
    j.at("secs").get_to(v.secs);
    j.at("nanosecs").get_to(v.nanosecs);
}

void from_json(const nlohmann::json &j, oci_config::linux_t::device_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::network_device_t &v)
{
    if (auto it = j.find("name"); it != j.end() && !it->is_null()) {
        it->get_to(v.name.emplace());
    }
}

// --- allowed_device_t ---

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::device_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::memory_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::cpu_t &v)
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
        auto val = it->get<int64_t>();
        v.idle.emplace((val == 1) ? oci_config::linux_t::resources_t::cpu_t::idle_t::IDLE
                                  : oci_config::linux_t::resources_t::cpu_t::idle_t::NONE);
    }
}

void from_json(const nlohmann::json &j,
               oci_config::linux_t::resources_t::block_io_t::weight_device_t &v)
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
               oci_config::linux_t::resources_t::block_io_t::throttle_device_t &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);
    j.at("rate").get_to(v.rate);
}

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::block_io_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::hugepage_limit_t &v)
{
    j.at("pageSize").get_to(v.page_size);
    j.at("limit").get_to(v.limit);
}

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::network_t::priority_t &v)
{
    j.at("name").get_to(v.name);
    j.at("priority").get_to(v.priority);
}

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::network_t &v)
{
    if (auto it = j.find("classID"); it != j.end() && !it->is_null()) {
        it->get_to(v.class_id.emplace());
    }

    if (auto it = j.find("priorities"); it != j.end() && !it->is_null()) {
        it->get_to(v.priorities.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::pids_t &v)
{
    if (auto it = j.find("limit"); it != j.end() && !it->is_null()) {
        it->get_to(v.limit.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t::rdma_t &v)
{
    if (auto it = j.find("hcaHandles"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_handles.emplace());
    }

    if (auto it = j.find("hcaObjects"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_objects.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::linux_t::intel_rdt_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::resources_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::memory_policy_t &v)
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
        oci_config::linux_t::memory_policy_t::flag_t flags{ };
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

void from_json(const nlohmann::json &j, oci_config::linux_t::personality_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::seccomp_t::syscall_t::arg_t &v)
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

void from_json(const nlohmann::json &j, oci_config::linux_t::seccomp_t::syscall_t &v)
{
    j.at("names").get_to(v.names);

    auto action_name = j.at("action").get<std::string_view>();
    auto action_opt = seccomp_action_table.from_name(action_name);
    if (UNLIKELY(!action_opt)) {
        throw std::runtime_error("unknown value: " + std::string(action_name));
    }
    v.action = *action_opt;

    if (auto it = j.find("errnoRet"); it != j.end() && !it->is_null()) {
        it->get_to(v.errno_ret.emplace());
    }

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::linux_t::seccomp_t &v)
{

    auto action_name = j.at("defaultAction").get<std::string_view>();
    auto action_opt = seccomp_action_table.from_name(action_name);
    if (UNLIKELY(!action_opt)) {
        throw std::runtime_error("unknown value: " + std::string(action_name));
    }
    v.default_action = *action_opt;

    if (auto it = j.find("defaultErrnoRet"); it != j.end() && !it->is_null()) {
        it->get_to(v.default_errno_ret.emplace());
    }

    if (auto it = j.find("architectures"); it != j.end() && !it->is_null()) {
        std::vector<oci_config::linux_t::seccomp_t::arch_t> archs;
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
        auto flags = oci_config::linux_t::seccomp_t::flag_t::NONE;
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

    if (auto it = j.find("syscalls"); it != j.end() && !it->is_null()) {
        it->get_to(v.syscalls.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::linux_t &v)
{
    if (auto it = j.find("uidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.uid_mappings.emplace());
    }

    if (auto it = j.find("gidMappings"); it != j.end() && !it->is_null()) {
        it->get_to(v.gid_mappings.emplace());
    }

    if (auto it = j.find("namespaces"); it != j.end() && !it->is_null()) {
        it->get_to(v.namespaces.emplace());

        // namespace type uniqueness is checked in validate()
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

        struct propagation_entry_t
        {
            std::string_view name;
            unsigned long value;
        };

        const std::array<propagation_entry_t, 8> table{ {
          { "private", MS_PRIVATE },
          { "rprivate", MS_PRIVATE | MS_REC },
          { "shared", MS_SHARED },
          { "rshared", MS_SHARED | MS_REC },
          { "slave", MS_SLAVE },
          { "rslave", MS_SLAVE | MS_REC },
          { "unbindable", MS_UNBINDABLE },
          { "runbindable", MS_UNBINDABLE | MS_REC },
        } };

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
            auto [iter, ignored] =
              offsets.try_emplace(clock_name,
                                  offset_json.get<oci_config::linux_t::time_offset_t>());
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

    if (auto it = j.find("seccomp"); it != j.end() && !it->is_null()) {
        it->get_to(v.seccomp.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::hooks_t::hook_t &v)
{
    j.at("path").get_to(v.path);

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }

    if (auto it = j.find("env"); it != j.end() && !it->is_null()) {
        it->get_to(v.env.emplace());
    }

    if (auto it = j.find("timeout"); it != j.end() && !it->is_null()) {
        it->get_to(v.timeout.emplace());
    }
}

void from_json(const nlohmann::json &j, oci_config::hooks_t &v)
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

void from_json(const nlohmann::json &j, oci_config::mount_t &v)
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
        std::tie(v.vfs_flags,
                 v.propagation_flags,
                 v.rec_attr,
                 v.extension_flags,
                 v.idmap,
                 v.uid_mappings,
                 v.gid_mappings,
                 v.data) = parse_mount_options(options);
    }
}

void from_json(const nlohmann::json &j, oci_config::root_t &v)
{
    j.at("path").get_to(v.path);
    v.readonly = j.value("readonly", false);
}

void from_json(const nlohmann::json &j, oci_config &v)
{
    auto semver = linyaps_box::utils::semver(j.at("ociVersion").get_ref<const std::string &>());
    if (UNLIKELY(!linyaps_box::utils::semver(oci_config::version).is_compatible_with(semver))) {
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

auto to_string_view(oci_config::linux_t::namespace_t::type type) noexcept -> std::string_view
{
    return namespace_type_table.to_name(type).value_or(std::string_view{ });
}

auto to_string_view(linyaps_box::oci_config::process_t::rlimit_t::type_t type) noexcept
  -> std::string_view
{
    return rlimit_type_table.to_name(type).value_or(std::string_view{ });
}

auto validate_namespace_path(const oci_config::linux_t::namespace_t &ns) -> void
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

auto oci_config::parse(std::string_view content) -> oci_config
{
    auto config = nlohmann::json::parse(content).get<oci_config>();
    validate(config);
    return config;
}

auto oci_config::parse(const std::filesystem::path &path) -> oci_config
{
    std::ifstream stream{ path, std::ios::binary | std::ios::in };
    if (UNLIKELY(!stream.is_open())) {
        throw std::filesystem::filesystem_error(
          "failed to open oci_config file",
          path,
          std::make_error_code(static_cast<std::errc>(errno)));
    }

    auto config = nlohmann::json::parse(stream).get<oci_config>();
    validate(config);
    return config;
}

} // namespace linyaps_box
