// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config.h"

#include "linyaps_box/utils/semver.h"
#include "nlohmann/json.hpp"

#include <sys/resource.h>

namespace {

// This function is used to parse the mount options from the config file and it only will be called
// once.
auto parse_mount_options(const std::vector<std::string> &options) -> std::
        tuple<unsigned long, unsigned long, linyaps_box::config::mount_t::extension, std::string>
{
    const std::unordered_map<std::string_view, unsigned long> propagation_flags_map{
        { "rprivate", MS_PRIVATE | MS_REC },       { "private", MS_PRIVATE },
        { "rslave", MS_SLAVE | MS_REC },           { "slave", MS_SLAVE },
        { "rshared", MS_SHARED | MS_REC },         { "shared", MS_SHARED },
        { "runbindable", MS_UNBINDABLE | MS_REC }, { "unbindable", MS_UNBINDABLE },
    };

    const std::unordered_map<std::string_view, unsigned long> flags_map{
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

    const std::unordered_map<std::string_view, unsigned long> unset_flags_map{
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

    const std::unordered_map<std::string_view, linyaps_box::config::mount_t::extension>
            extra_flags_map{ { "copy-symlink",
                               linyaps_box::config::mount_t::extension::COPY_SYMLINK } };

    unsigned long flags = 0;
    linyaps_box::config::mount_t::extension extra_flags{
        linyaps_box::config::mount_t::extension::NONE
    };

    unsigned long propagation_flags = 0;
    std::stringstream data;

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

        data << "," << opt;
    }
    auto str = data.str();
    if (!str.empty()) {
        str = str.substr(1);
    }

    return { flags, propagation_flags, extra_flags, str };
}

#ifdef LINYAPS_BOX_ENABLE_CAP
linyaps_box::config::process_t::capabilities_t
parse_capability(const nlohmann::json &obj, const nlohmann::json::json_pointer &ptr)
{
    auto parse_cap_set = [&obj, &ptr](const char *set_name) {
        const auto set = ptr / set_name;
        std::vector<cap_value_t> cap_list;
        if (!obj.contains(set)) {
            return cap_list;
        }

        const auto vec = obj[set].get<std::vector<std::string>>();
        std::for_each(vec.cbegin(), vec.cend(), [&cap_list](const std::string &cap) {
            cap_value_t val{ 0 };
            if (cap_from_name(cap.c_str(), &val) < 0) {
                throw std::runtime_error("unknown capability: " + cap);
            }

            cap_list.push_back(val);
        });

        return cap_list;
    };

    linyaps_box::config::process_t::capabilities_t cap{};
    cap.effective = parse_cap_set("effective");
    cap.ambient = parse_cap_set("ambient");
    cap.bounding = parse_cap_set("bounding");
    cap.inheritable = parse_cap_set("inheritable");
    cap.permitted = parse_cap_set("permitted");

    return cap;
}
#endif

linyaps_box::config::process_t::rlimits_t parse_rlimits(const nlohmann::json &obj,
                                                        const nlohmann::json::json_pointer &ptr)
{
    const auto &vec = obj[ptr];
    if (!vec.is_array()) {
        throw std::runtime_error("rlimits must be an array");
    }

    linyaps_box::config::process_t::rlimits_t ret{};
    std::transform(
            vec.cbegin(),
            vec.cend(),
            std::back_inserter(ret),
            [](const nlohmann::json &json) {
                if (!json.is_object()) {
                    throw std::runtime_error("rlimit must be an object");
                }

                if (!json.contains("type")) {
                    throw std::runtime_error("rlimit must contain type");
                }

                return linyaps_box::config::process_t::rlimit_t{ json["type"].get<std::string>(),
                                                                 json["soft"].get<uint64_t>(),
                                                                 json["hard"].get<uint64_t>() };
            });
    return ret;
}

auto parse_linux(const nlohmann::json &obj, const nlohmann::json::json_pointer &ptr)
        -> linyaps_box::config::linux_t
{
    auto linux = linyaps_box::config::linux_t{};
    if (auto uid_ptr = ptr / "uidMappings"; obj.contains(uid_ptr)) {
        const auto &vec = obj[uid_ptr];
        std::vector<linyaps_box::config::linux_t::id_mapping_t> uid_mappings;
        std::transform(vec.cbegin(),
                       vec.cend(),
                       std::back_inserter(uid_mappings),
                       [](const nlohmann::json &json) {
                           return linyaps_box::config::linux_t::id_mapping_t{
                               json["hostID"].get<uid_t>(),
                               json["containerID"].get<uid_t>(),
                               json["size"].get<size_t>(),
                           };
                       });
        linux.uid_mappings = std::move(uid_mappings);
    }

    if (auto gid_ptr = ptr / "gidMappings"; obj.contains(gid_ptr)) {
        const auto &vec = obj[gid_ptr];
        std::vector<linyaps_box::config::linux_t::id_mapping_t> gid_mappings;
        std::transform(vec.cbegin(),
                       vec.cend(),
                       std::back_inserter(gid_mappings),
                       [](const nlohmann::json &json) {
                           return linyaps_box::config::linux_t::id_mapping_t{
                               json["hostID"].get<uid_t>(),
                               json["containerID"].get<uid_t>(),
                               json["size"].get<size_t>(),
                           };
                       });
        linux.gid_mappings = std::move(gid_mappings);
    }

    if (auto namespace_ptr = ptr / "namespaces"; obj.contains(namespace_ptr)) {
        auto map_fn = [](const nlohmann::json &json) {
            if (!json.contains("type")) {
                throw std::runtime_error("property `type` is REQUIRED for linux namespaces");
            }

            linyaps_box::config::linux_t::namespace_t n;
            auto type = json["type"].get<std::string>();
            if (type == "pid") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::PID;
            } else if (type == "network") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::NET;
            } else if (type == "ipc") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::IPC;
            } else if (type == "uts") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::UTS;
            } else if (type == "mount") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::MOUNT;
            } else if (type == "user") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::USER;
            } else if (type == "cgroup") {
                n.type_ = linyaps_box::config::linux_t::namespace_t::type::CGROUP;
            } else {
                throw std::runtime_error("unsupported namespace type: " + type);
            }

            if (json.contains("path")) {
                n.path = json["path"].get<std::string>();
            }

            return n;
        };

        const auto &vec = obj[namespace_ptr];
        std::vector<linyaps_box::config::linux_t::namespace_t> ns;
        std::transform(vec.cbegin(), vec.cend(), std::back_inserter(ns), map_fn);

        linux.namespaces = std::move(ns);
    }

    if (auto masked_path = ptr / "maskedPaths"; obj.contains(masked_path)) {
        const auto &vec = obj[masked_path];
        std::vector<std::filesystem::path> masked_paths;
        std::transform(vec.cbegin(),
                       vec.cend(),
                       std::back_inserter(masked_paths),
                       [](const nlohmann::json &json) {
                           return json.get<std::string>();
                       });
        linux.masked_paths = std::move(masked_paths);
    }

    if (auto readonly_path = ptr / "readonlyPaths"; obj.contains(readonly_path)) {
        const auto &vec = obj[readonly_path];
        std::vector<std::filesystem::path> readonly_paths;
        std::transform(vec.cbegin(),
                       vec.cend(),
                       std::back_inserter(readonly_paths),
                       [](const nlohmann::json &json) {
                           return json.get<std::string>();
                       });
        linux.readonly_paths = std::move(readonly_paths);
    }

    if (auto rootfs_propagation = ptr / "rootfsPropagation"; obj.contains(rootfs_propagation)) {
        auto val = obj[rootfs_propagation].get<std::string>();
        if (val == "shared") {
            linux.rootfs_propagation = MS_SHARED;
        } else if (val == "slave") {
            linux.rootfs_propagation = MS_SLAVE;
        } else if (val == "private") {
            linux.rootfs_propagation = MS_PRIVATE;
        } else if (val == "unbindable") {
            linux.rootfs_propagation = MS_UNBINDABLE;
        } else {
            throw std::runtime_error("unsupported rootfs propagation: " + val);
        }
    }

    return linux;
}

auto parse_1_2_0(const nlohmann::json &j) -> linyaps_box::config
{
    const auto ptr = ""_json_pointer;

    auto semver = linyaps_box::utils::semver(j[ptr / "ociVersion"].get<std::string>());
    if (!linyaps_box::utils::semver(linyaps_box::config::oci_version).is_compatible_with(semver)) {
        throw std::runtime_error("unsupported OCI version: " + semver.to_string());
    }

    linyaps_box::config cfg;

    {
        if (j.contains(ptr / "process" / "terminal")) {
            cfg.process.terminal = j[ptr / "process" / "terminal"].get<bool>();
        }

        // https://github.com/opencontainers/runtime-spec/blob/09fcb39bb7185b46dfb206bc8f3fea914c674779/config.md?plain=1#L245
        if (cfg.process.terminal && j.contains(ptr / "process" / "consoleSize")) {
            cfg.process.console_size = linyaps_box::config::process_t::console_size_t{
                j[ptr / "process" / "consoleSize" / "height"].get<unsigned short>(),
                j[ptr / "process" / "consoleSize" / "width"].get<unsigned short>()
            };
        }

        cfg.process.cwd = j[ptr / "process" / "cwd"].get<std::string>();

        if (j.contains(ptr / "process" / "env")) {
            auto env = j[ptr / "process" / "env"].get<std::vector<std::string>>();
            for (const auto &e : env) {
                auto pos = e.find('=');
                if (pos == std::string::npos) {
                    throw std::runtime_error("invalid env entry: " + e);
                }
            }

            cfg.process.env = std::move(env);
        }

        cfg.process.args = j[ptr / "process" / "args"].get<std::vector<std::string>>();

        if (auto rlimits = ptr / "process" / "rlimits"; j.contains(rlimits)) {
            cfg.process.rlimits = parse_rlimits(j, rlimits);
        }

        // TODO: apparmorProfile
#ifdef LINYAPS_BOX_ENABLE_CAP
        if (auto cap = ptr / "process" / "capabilities"; j.contains(cap)) {
            cfg.process.capabilities = parse_capability(j, cap);
        }
#endif

        if (j.contains(ptr / "process" / "noNewPrivileges")) {
            cfg.process.no_new_privileges = j[ptr / "process" / "noNewPrivileges"].get<bool>();
        }

        if (j.contains(ptr / "process" / "oomScoreAdj")) {
            cfg.process.oom_score_adj = j[ptr / "process" / "oomScoreAdj"].get<int>();
        }

        cfg.process.user.uid = j[ptr / "process" / "user" / "uid"].get<uid_t>();
        cfg.process.user.gid = j[ptr / "process" / "user" / "gid"].get<gid_t>();

        if (j.contains(ptr / "process" / "user" / "umask")) {
            cfg.process.user.umask = j[ptr / "process" / "user" / "umask"].get<mode_t>();
        }

        if (j.contains(ptr / "process" / "user" / "additionalGids")) {
            cfg.process.user.additional_gids =
                    j[ptr / "process" / "user" / "additionalGids"].get<std::vector<gid_t>>();
        }
    }

    if (auto linux_ptr = ptr / "linux"; j.contains(linux_ptr)) {
        cfg.linux = parse_linux(j, linux_ptr);
    }

    if (j.contains(ptr / "hooks")) {
        auto hooks = j[ptr / "hooks"];
        auto get_hooks = [&](const std::string &key)
                -> std::optional<std::vector<linyaps_box::config::hooks_t::hook_t>> {
            if (!hooks.contains(key)) {
                return std::nullopt;
            }

            std::vector<linyaps_box::config::hooks_t::hook_t> result;
            for (const auto &h : hooks[key]) {
                linyaps_box::config::hooks_t::hook_t hook;
                hook.path = h["path"].get<std::string>();
                if (!hook.path.is_absolute()) {
                    throw std::runtime_error(key + "path must be absolute");
                }

                if (h.contains("args")) {
                    hook.args = h["args"].get<std::vector<std::string>>();
                }

                if (h.contains("env")) {
                    std::unordered_map<std::string, std::string> env;

                    for (const auto &e : h["env"].get<std::vector<std::string>>()) {
                        auto pos = e.find('=');
                        if (pos == std::string::npos) {
                            throw std::runtime_error("invalid env entry: " + e);
                        }

                        env[e.substr(0, pos)] = e.substr(pos + 1);
                    }

                    hook.env = std::move(env);
                }

                if (h.contains("timeout")) {
                    hook.timeout = h["timeout"].get<int>();
                    if (hook.timeout <= 0) {
                        throw std::runtime_error(key + "timeout must be greater than zero");
                    }
                }

                result.push_back(hook);
            }

            return result;
        };

        cfg.hooks.prestart = get_hooks("prestart");
        cfg.hooks.create_runtime = get_hooks("createRuntime");
        cfg.hooks.create_container = get_hooks("createContainer");
        cfg.hooks.start_container = get_hooks("startContainer");
        cfg.hooks.poststart = get_hooks("poststart");
        cfg.hooks.poststop = get_hooks("poststop");
    }

    if (j.contains(ptr / "mounts")) {
        std::vector<linyaps_box::config::mount_t> mounts;
        for (const auto &m : j[ptr / "mounts"]) {
            linyaps_box::config::mount_t mount;
            if (m.contains("source")) {
                mount.source = m["source"].get<std::string>();
            }
            if (m.contains("destination")) {
                mount.destination = m["destination"].get<std::string>();
            }
            mount.type = m["type"].get<std::string>();

            const auto it = m.find("options");
            if (it != m.end()) {
                auto options = it->get<std::vector<std::string>>();
                std::tie(mount.flags, mount.propagation_flags, mount.extension_flags, mount.data) =
                        parse_mount_options(options);
            }

            mounts.push_back(mount);
        }
        cfg.mounts = mounts;
    }

    auto root = ptr / "root";
    if (!j.contains(root)) {
        throw std::runtime_error("root must be specified");
    }

    if (!j.contains(root / "path")) {
        throw std::runtime_error("root.path must be specified");
    }
    cfg.root.path = j[root / "path"].get<std::filesystem::path>();

    if (j.contains(root / "readonly")) {
        cfg.root.readonly = j[root / "readonly"].get<bool>();
    }

    auto annotations = ptr / "annotations";
    if (j.contains(annotations)) {
        cfg.annotations = j[annotations].get<std::unordered_map<std::string, std::string>>();
    }

    return cfg;
}

} // namespace

linyaps_box::config linyaps_box::config::parse(std::istream &is)
{
    auto j = nlohmann::json::parse(is);
    return parse_1_2_0(j);
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
