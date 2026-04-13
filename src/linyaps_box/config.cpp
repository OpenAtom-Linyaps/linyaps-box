// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config.h"

#include "linyaps_box/utils/semver.h"
#include "nlohmann/json.hpp"

#include <sys/resource.h>

namespace {

// Maps are static to avoid re-construction on every mount entry.
static const std::unordered_map<std::string_view, unsigned long> propagation_flags_map{
    { "rprivate", MS_PRIVATE | MS_REC },       { "private", MS_PRIVATE },
    { "rslave", MS_SLAVE | MS_REC },           { "slave", MS_SLAVE },
    { "rshared", MS_SHARED | MS_REC },         { "shared", MS_SHARED },
    { "runbindable", MS_UNBINDABLE | MS_REC }, { "unbindable", MS_UNBINDABLE },
};

static const std::unordered_map<std::string_view, unsigned long> flags_map{
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

static const std::unordered_map<std::string_view, unsigned long> unset_flags_map{
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

static const std::unordered_map<std::string_view, linyaps_box::config::mount_t::extension>
        extra_flags_map{
            { "copy-symlink", linyaps_box::config::mount_t::extension::COPY_SYMLINK },
        };

// NOLINTEND(cert-err58-cpp)

auto parse_mount_options(const std::vector<std::string> &options) -> std::
        tuple<unsigned long, unsigned long, linyaps_box::config::mount_t::extension, std::string>
{
    unsigned long flags = 0;
    auto extra_flags = linyaps_box::config::mount_t::extension::NONE;
    unsigned long propagation_flags = 0;
    std::string data;

    for (const auto &opt : options) {
        if (auto it = flags_map.find(opt); it != flags_map.end()) {
            flags |= it->second;
            continue;
        }
        if (auto it = unset_flags_map.find(opt); it != unset_flags_map.end()) {
            flags &= ~it->second;
            continue;
        }
        if (auto it = propagation_flags_map.find(opt); it != propagation_flags_map.end()) {
            propagation_flags |= it->second;
            continue;
        }
        if (auto it = extra_flags_map.find(opt); it != extra_flags_map.end()) {
            extra_flags |= it->second;
            continue;
        }

        if (!data.empty()) {
            data += ',';
        }
        data += opt;
    }

    return { flags, propagation_flags, extra_flags, std::move(data) };
}

static const std::unordered_map<std::string_view, linyaps_box::config::linux_t::namespace_t::type>
        namespace_type_map{
            { "pid", linyaps_box::config::linux_t::namespace_t::type::PID },
            { "network", linyaps_box::config::linux_t::namespace_t::type::NET },
            { "ipc", linyaps_box::config::linux_t::namespace_t::type::IPC },
            { "uts", linyaps_box::config::linux_t::namespace_t::type::UTS },
            { "mount", linyaps_box::config::linux_t::namespace_t::type::MOUNT },
            { "user", linyaps_box::config::linux_t::namespace_t::type::USER },
            { "cgroup", linyaps_box::config::linux_t::namespace_t::type::CGROUP },
        };

static const std::unordered_map<std::string_view, unsigned int> rootfs_propagation_map{
    { "shared", MS_SHARED },
    { "slave", MS_SLAVE },
    { "private", MS_PRIVATE },
    { "unbindable", MS_UNBINDABLE },
};

} // namespace

// ADL-based from_json overloads in the linyaps_box namespace.
// nlohmann_json discovers these via argument-dependent lookup when calling j.get<T>().
namespace linyaps_box {

void from_json(const nlohmann::json &j, config::process_t::console_size_t &v)
{
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

void from_json(const nlohmann::json &j, config::process_t::rlimit_t &v)
{
    if (!j.contains("type")) {
        throw std::runtime_error("rlimit must contain type");
    }
    j.at("type").get_to(v.type);
    j.at("soft").get_to(v.soft);
    j.at("hard").get_to(v.hard);
}

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
        for (const auto &elem : *it) {
            cap_value_t val{ 0 };
            auto cap_name = elem.get_ref<const std::string &>();
            if (cap_from_name(cap_name.c_str(), &val) < 0) {
                throw std::runtime_error("unknown capability: " + cap_name);
            }
            result.push_back(val);
        }
        return result;
    };

    v.effective = parse_cap_set("effective");
    v.ambient = parse_cap_set("ambient");
    v.bounding = parse_cap_set("bounding");
    v.inheritable = parse_cap_set("inheritable");
    v.permitted = parse_cap_set("permitted");
}
#endif

void from_json(const nlohmann::json &j, config::process_t &v)
{
    if (auto it = j.find("terminal"); it != j.end()) {
        it->get_to(v.terminal);
    }

    // https://github.com/opencontainers/runtime-spec/blob/09fcb39bb7185b46dfb206bc8f3fea914c674779/config.md?plain=1#L245
    if (v.terminal) {
        if (auto it = j.find("consoleSize"); it != j.end()) {
            v.console_size = it->get<config::process_t::console_size_t>();
        }
    }

    j.at("cwd").get_to(v.cwd);

    if (auto it = j.find("env"); it != j.end()) {
        auto env = it->get<std::vector<std::string>>();
        for (const auto &e : env) {
            if (e.find('=') == std::string::npos) {
                throw std::runtime_error("invalid env entry: " + e);
            }
        }
        v.env = std::move(env);
    }

    j.at("args").get_to(v.args);

    if (auto it = j.find("rlimits"); it != j.end()) {
        v.rlimits = it->get<config::process_t::rlimits_t>();
    }

    // TODO: apparmorProfile
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

    j.at("user").get_to(v.user);
}

void from_json(const nlohmann::json &j, config::linux_t::id_mapping_t &v)
{
    j.at("hostID").get_to(v.host_id);
    j.at("containerID").get_to(v.container_id);
    j.at("size").get_to(v.size);
}

void from_json(const nlohmann::json &j, config::linux_t::namespace_t &v)
{
    auto type_str = j.at("type").get_ref<const std::string &>();
    auto it = namespace_type_map.find(type_str);
    if (it == namespace_type_map.end()) {
        throw std::runtime_error("unsupported namespace type: " + type_str);
    }
    v.type_ = it->second;

    if (auto path_it = j.find("path"); path_it != j.end()) {
        v.path = path_it->get<std::string>();
    }
}

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
    }

    if (auto it = j.find("maskedPaths"); it != j.end()) {
        v.masked_paths = it->get<std::vector<std::filesystem::path>>();
    }

    if (auto it = j.find("readonlyPaths"); it != j.end()) {
        v.readonly_paths = it->get<std::vector<std::filesystem::path>>();
    }

    if (auto it = j.find("rootfsPropagation"); it != j.end()) {
        auto val = it->get_ref<const std::string &>();
        auto prop_it = rootfs_propagation_map.find(val);
        if (prop_it == rootfs_propagation_map.end()) {
            throw std::runtime_error("unsupported rootfs propagation: " + val);
        }
        v.rootfs_propagation = prop_it->second;
    }
}

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
        for (const auto &e : it->get<std::vector<std::string>>()) {
            auto pos = e.find('=');
            if (pos == std::string::npos) {
                throw std::runtime_error("invalid env entry: " + e);
            }
            env[e.substr(0, pos)] = e.substr(pos + 1);
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

void from_json(const nlohmann::json &j, config::mount_t &v)
{
    if (auto it = j.find("source"); it != j.end()) {
        v.source = it->get<std::string>();
    }

    if (auto it = j.find("destination"); it != j.end()) {
        v.destination = it->get<std::string>();
    }

    j.at("type").get_to(v.type);

    if (auto it = j.find("options"); it != j.end()) {
        auto options = it->get<std::vector<std::string>>();
        std::tie(v.flags, v.propagation_flags, v.extension_flags, v.data) =
                parse_mount_options(options);
    }
}

void from_json(const nlohmann::json &j, config::root_t &v)
{
    j.at("path").get_to(v.path);

    if (auto it = j.find("readonly"); it != j.end()) {
        it->get_to(v.readonly);
    }
}

void from_json(const nlohmann::json &j, config &v)
{
    auto semver = linyaps_box::utils::semver(j.at("ociVersion").get_ref<const std::string &>());
    if (!linyaps_box::utils::semver(config::oci_version).is_compatible_with(semver)) {
        throw std::runtime_error("unsupported OCI version: " + semver.to_string());
    }

    j.at("process").get_to(v.process);

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

} // namespace linyaps_box

linyaps_box::config linyaps_box::config::parse(std::string_view content)
{
    return nlohmann::json::parse(content).get<linyaps_box::config>();
}

std::string linyaps_box::to_string(linyaps_box::config::linux_t::namespace_t::type type) noexcept
{
    switch (type) {
    case config::linux_t::namespace_t::type::NONE:
        return "none";
    case config::linux_t::namespace_t::type::IPC:
        return "ipc";
    case config::linux_t::namespace_t::type::UTS:
        return "uts";
    case config::linux_t::namespace_t::type::MOUNT:
        return "mount";
    case config::linux_t::namespace_t::type::PID:
        return "pid";
    case config::linux_t::namespace_t::type::NET:
        return "net";
    case config::linux_t::namespace_t::type::USER:
        return "user";
    case config::linux_t::namespace_t::type::CGROUP:
        return "cgroup";
    }

    __builtin_unreachable();
}
