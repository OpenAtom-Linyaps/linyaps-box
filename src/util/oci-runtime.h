/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <sys/mount.h>

#include "util.h"

namespace linglong {

#define LLJS_FROM(KEY) (o.KEY = j.at(#KEY).get<decltype(o.KEY)>())
#define LLJS_FROM_OPT(KEY) (o.KEY = optional<decltype(o.KEY)::value_type>(j, #KEY))

#define LLJS_TO(KEY) (j[#KEY] = o.KEY)

#define LLJS_FROM_OBJ(TYPE) inline void from_json(const nlohmann::json &j, TYPE &o)
#define LLJS_TO_OBJ(TYPE) inline void to_json(nlohmann::json &j, const TYPE &o)

#undef linux

struct Root {
    std::string path;
    tl::optional<bool> readonly;
};

LLJS_FROM_OBJ(Root)
{
    LLJS_FROM(path);
    LLJS_FROM_OPT(readonly);
}

LLJS_TO_OBJ(Root)
{
    LLJS_TO(path);
    LLJS_TO(readonly);
}

struct Process {
    util::str_vec args;
    util::str_vec env;
    std::string cwd;
};

inline void from_json(const nlohmann::json &j, Process &o)
{
    o.args = j.at("args").get<util::str_vec>();
    o.env = j.at("env").get<util::str_vec>();
    o.cwd = j.at("cwd").get<std::string>();
}

inline void to_json(nlohmann::json &j, const Process &o)
{
    j["args"] = o.args;
    j["env"] = o.env;
    j["cwd"] = o.cwd;
}

struct Mount {
    enum Type {
        Unknown,
        Bind,
        Proc,
        Sysfs,
        Devpts,
        Mqueue,
        Tmpfs,
        Cgroup,
        Cgroup2,
    };
    std::string destination;
    std::string type;
    std::string source;
    util::str_vec options;

    Type fsType;
    uint32_t flags = 0u;
};

inline void from_json(const nlohmann::json &j, Mount &o)
{
    static std::map<std::string, Mount::Type> fsTypes = {
        {"bind", Mount::Bind},   {"proc", Mount::Proc},   {"devpts", Mount::Devpts}, {"mqueue", Mount::Mqueue},
        {"tmpfs", Mount::Tmpfs}, {"sysfs", Mount::Sysfs}, {"cgroup", Mount::Cgroup}, {"cgroup2", Mount::Cgroup2},
    };

    static std::map<std::string, uint32_t> optionFlags = {
        {"nosuid", MS_NOSUID}, {"strictatime", MS_STRICTATIME}, {"noexec", MS_NOEXEC},
        {"nodev", MS_NODEV},   {"relatime", MS_RELATIME},       {"ro", MS_RDONLY},
    };

    o.destination = j.at("destination").get<std::string>();
    o.type = j.at("type").get<std::string>();
    o.fsType = fsTypes.find(o.type)->second;
    o.source = j.at("source").get<std::string>();

    o.options = j.value("options", util::str_vec());
    if (o.options.size() > 0) {
        for (auto const &opt : o.options) {
            if (optionFlags.find(opt) != optionFlags.end()) {
                o.flags |= optionFlags.find(opt)->second;
            }
        }
    }
}

inline void to_json(nlohmann::json &j, const Mount &o)
{
    j["destination"] = o.destination;
    j["source"] = o.source;
    j["type"] = o.type;
    j["options"] = o.options;
}

struct Namespace {
    int type;
};

static std::map<std::string, int> namespaceType = {
    {"pid", CLONE_NEWPID},     {"uts", CLONE_NEWUTS}, {"mount", CLONE_NEWNS},  {"cgroup", CLONE_NEWCGROUP},
    {"network", CLONE_NEWNET}, {"ipc", CLONE_NEWIPC}, {"user", CLONE_NEWUSER},
};

inline void from_json(const nlohmann::json &j, Namespace &o)
{
    o.type = namespaceType.find(j.at("type").get<std::string>())->second;
}

inline void to_json(nlohmann::json &j, const Namespace &o)
{
    auto matchPair = std::find_if(std::begin(namespaceType), std::end(namespaceType),
                                  [&](const std::pair<std::string, int> &pair) { return pair.second == o.type; });
    j["type"] = matchPair->first;
}

struct IDMap {
    uint64_t containerID = 0u;
    uint64_t hostID = 0u;
    uint64_t size = 0u;
};

inline void from_json(const nlohmann::json &j, IDMap &o)
{
    o.hostID = j.value("hostID", 0);
    o.containerID = j.value("containerID", 0);
    o.size = j.value("size", 0);
}

inline void to_json(nlohmann::json &j, const IDMap &o)
{
    j["hostID"] = o.hostID;
    j["containerID"] = o.containerID;
    j["size"] = o.size;
}

typedef std::string SeccompAction;
typedef std::string SeccompArch;

struct SyscallArg {
    u_int index; // require
    u_int64_t value; // require
    u_int64_t valueTwo; // optional
    std::string op; // require
};

inline void from_json(const nlohmann::json &j, SyscallArg &o)
{
    o.index = j.at("index").get<u_int>();
    o.value = j.at("value").get<u_int64_t>();
    o.valueTwo = j.value("valueTwo", u_int64_t());
    o.op = j.at("op").get<std::string>();
}

inline void to_json(nlohmann::json &j, const SyscallArg &o)
{
    j["index"] = o.index;
    j["value"] = o.value;
    j["valueTwo"] = o.valueTwo;
    j["op"] = o.op;
}

struct Syscall {
    util::str_vec names;
    SeccompAction action;
    std::vector<SyscallArg> args;
};

inline void from_json(const nlohmann::json &j, Syscall &o)
{
    o.names = j.at("names").get<util::str_vec>();
    o.action = j.at("action").get<SeccompAction>();
    o.args = j.value("args", std::vector<SyscallArg>());
}

inline void to_json(nlohmann::json &j, const Syscall &o)
{
    j["names"] = o.names;
    j["action"] = o.action;
    j["args"] = o.args;
}

struct Seccomp {
    SeccompAction defaultAction = "INVALID_ACTION";
    std::vector<SeccompArch> architectures;
    std::vector<Syscall> syscalls;
};

inline void from_json(const nlohmann::json &j, Seccomp &o)
{
    o.defaultAction = j.at("defaultAction").get<std::string>();
    o.architectures = j.value("architectures", std::vector<SeccompArch> {});
    o.syscalls = j.value("syscalls", std::vector<Syscall> {});
}

inline void to_json(nlohmann::json &j, const Seccomp &o)
{
    j["defaultAction"] = o.defaultAction;
    j["architectures"] = o.architectures;
    j["syscalls"] = o.syscalls;
}

// https://github.com/containers/crun/blob/main/crun.1.md#memory-controller
struct ResourceMemory {
    int64_t limit = -1;
    int64_t reservation = -1;
    int64_t swap = -1;
};

inline void from_json(const nlohmann::json &j, ResourceMemory &o)
{
    o.limit = j.value("limit", -1);
    o.reservation = j.value("reservation", -1);
    o.swap = j.value("swap", -1);
}

inline void to_json(nlohmann::json &j, const ResourceMemory &o)
{
    j["limit"] = o.limit;
    j["reservation"] = o.reservation;
    j["swap"] = o.swap;
}

// https://github.com/containers/crun/blob/main/crun.1.md#cpu-controller
// support v1 and v2 with conversion
struct ResourceCPU {
    u_int64_t shares = 1024;
    int64_t quota = 100000;
    u_int64_t period = 100000;
    //    int64_t realtimeRuntime;
    //    int64_t realtimePeriod;
    //    std::string cpus;
    //    std::string mems;
};

inline void from_json(const nlohmann::json &j, ResourceCPU &o)
{
    o.shares = j.value("shares", 1024);
    o.quota = j.value("quota", 100000);
    o.period = j.value("period", 100000);
}

inline void to_json(nlohmann::json &j, const ResourceCPU &o)
{
    j["shares"] = o.shares;
    j["quota"] = o.quota;
    j["period"] = o.period;
}

struct Resources {
    ResourceMemory memory;
    ResourceCPU cpu;
};

inline void from_json(const nlohmann::json &j, Resources &o)
{
    o.cpu = j.value("cpu", ResourceCPU());
    o.memory = j.value("memory", ResourceMemory());
}

inline void to_json(nlohmann::json &j, const Resources &o)
{
    j["cpu"] = o.cpu;
    j["memory"] = o.memory;
}

struct Linux {
    std::vector<Namespace> namespaces;
    std::vector<IDMap> uidMappings;
    std::vector<IDMap> gidMappings;
    tl::optional<Seccomp> seccomp;
    std::string cgroupsPath;
    Resources resources;
};

inline void from_json(const nlohmann::json &j, Linux &o)
{
    o.namespaces = j.at("namespaces").get<std::vector<Namespace>>();
    o.uidMappings = j.value("uidMappings", std::vector<IDMap> {});
    o.gidMappings = j.value("gidMappings", std::vector<IDMap> {});
    o.seccomp = optional<decltype(o.seccomp)::value_type>(j, "seccomp");
    o.cgroupsPath = j.value("cgroupsPath", "");
    o.resources = j.value("resources", Resources());
}

inline void to_json(nlohmann::json &j, const Linux &o)
{
    j["namespaces"] = o.namespaces;
    j["uidMappings"] = o.uidMappings;
    j["gidMappings"] = o.gidMappings;
    j["seccomp"] = o.seccomp;
    j["cgroupsPath"] = o.cgroupsPath;
    j["resources"] = o.resources;
}

/*
    "hooks": {
        "prestart": [
            {
                "path": "/usr/bin/fix-mounts",
                "args": ["fix-mounts", "arg1", "arg2"],
                "env":  [ "key1=value1"]
            },
            {
                "path": "/usr/bin/setup-network"
            }
        ],
        "poststart": [
            {
                "path": "/usr/bin/notify-start",
                "timeout": 5
            }
        ],
        "poststop": [
            {
                "path": "/usr/sbin/cleanup.sh",
                "args": ["cleanup.sh", "-f"]
            }
        ]
    }
 */

struct Hook {
    std::string path;
    tl::optional<util::str_vec> args;
    tl::optional<std::vector<std::string>> env;
};

inline void from_json(const nlohmann::json &j, Hook &o)
{
    LLJS_FROM(path);
    LLJS_FROM_OPT(args);
    LLJS_FROM_OPT(env);
}

inline void to_json(nlohmann::json &j, const Hook &o)
{
    j["path"] = o.path;
    j["args"] = o.args;
    j["env"] = o.env;
}

struct Hooks {
    tl::optional<std::vector<Hook>> prestart;
    tl::optional<std::vector<Hook>> poststart;
    tl::optional<std::vector<Hook>> poststop;
};

inline void from_json(const nlohmann::json &j, Hooks &o)
{
    LLJS_FROM_OPT(prestart);
    LLJS_FROM_OPT(poststart);
    LLJS_FROM_OPT(poststop);
}

inline void to_json(nlohmann::json &j, const Hooks &o)
{
    j["poststop"] = o.poststop;
    j["poststart"] = o.poststart;
    j["prestart"] = o.prestart;
}

struct Runtime {
    std::string version;
    Root root;
    Process process;
    std::string hostname;
    Linux linux;
    tl::optional<std::vector<Mount>> mounts;
    tl::optional<Hooks> hooks;
};

inline void from_json(const nlohmann::json &j, Runtime &o)
{
    o.version = j.at("ociVersion").get<std::string>();
    o.hostname = j.at("hostname").get<std::string>();
    LLJS_FROM(process);
    o.mounts = optional<decltype(o.mounts)::value_type>(j, "mounts");
    LLJS_FROM(linux);
    // maybe optional
    LLJS_FROM(root);
    o.hooks = optional<decltype(o.hooks)::value_type>(j, "hooks");
}

inline void to_json(nlohmann::json &j, const Runtime &o)
{
    j["ociVersion"] = o.version;
    j["hostname"] = o.hostname;
    j["process"] = o.process;
    j["mounts"] = o.mounts;
    j["linux"] = o.linux;
    j["root"] = o.root;
    j["hooks"] = o.hooks;
}

inline static Runtime fromFile(const std::string &filepath)
{
    return util::json::fromFile(filepath).get<Runtime>();
}

inline static Runtime fromString(const std::string &content)
{
    return util::json::fromByteArray(content).get<Runtime>();
}

} // namespace linglong
