/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_OCI_RUNTIME_H_
#define LINGLONG_BOX_SRC_UTIL_OCI_RUNTIME_H_

#include <sys/mount.h>

#include "util.h"

namespace linglong {

#define LLJS_FROM(KEY) (LLJS_FROM_VARNAME(KEY, KEY))
#define LLJS_FROM_VARNAME(KEY, VAR) (o.VAR = j.at(#KEY).get<decltype(o.VAR)>())
#define LLJS_FROM_OPT(KEY) (LLJS_FROM_OPT_VARNAME(KEY, KEY))
#define LLJS_FROM_OPT_VARNAME(KEY, VAR) (o.VAR = optional<decltype(o.VAR)::value_type>(j, #KEY))

#define LLJS_TO(KEY) (LLJS_TO_VARNAME(KEY, KEY))
#define LLJS_TO_VARNAME(KEY, VAR) (j[#KEY] = o.VAR)

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
    util::strVec args;
    util::strVec env;
    std::string cwd;
};

inline void from_json(const nlohmann::json &j, Process &o)
{
    o.args = j.at("args").get<util::strVec>();
    o.env = j.at("env").get<util::strVec>();
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
        kUnknown,
        kBind,
        kProc,
        kSysfs,
        kDevpts,
        kMqueue,
        kTmpfs,
        kCgroup,
        kCgroup2,
    };
    std::string destination;
    std::string type;
    std::string source;
    util::strVec data;

    Type fsType;
    uint32_t flags = 0u;
};

inline void from_json(const nlohmann::json &j, Mount &o)
{
    static std::map<std::string, Mount::Type> fsTypes = {
        {"bind", Mount::kBind},   {"proc", Mount::kProc},   {"devpts", Mount::kDevpts}, {"mqueue", Mount::kMqueue},
        {"tmpfs", Mount::kTmpfs}, {"sysfs", Mount::kSysfs}, {"cgroup", Mount::kCgroup}, {"cgroup2", Mount::kCgroup2},
    };

    struct mountFlag {
        bool clear;
        uint32_t flag;
    };

    static std::map<std::string, mountFlag> optionFlags = {
        {"acl", {false, MS_POSIXACL}},
        {"async", {true, MS_SYNCHRONOUS}},
        {"atime", {true, MS_NOATIME}},
        {"bind", {false, MS_BIND}},
        {"defaults", {false, 0}},
        {"dev", {true, MS_NODEV}},
        {"diratime", {true, MS_NODIRATIME}},
        {"dirsync", {false, MS_DIRSYNC}},
        {"exec", {true, MS_NOEXEC}},
        {"iversion", {false, MS_I_VERSION}},
        {"lazytime", {false, MS_LAZYTIME}},
        {"loud", {true, MS_SILENT}},
        {"mand", {false, MS_MANDLOCK}},
        {"noacl", {true, MS_POSIXACL}},
        {"noatime", {false, MS_NOATIME}},
        {"nodev", {false, MS_NODEV}},
        {"nodiratime", {false, MS_NODIRATIME}},
        {"noexec", {false, MS_NOEXEC}},
        {"noiversion", {true, MS_I_VERSION}},
        {"nolazytime", {true, MS_LAZYTIME}},
        {"nomand", {true, MS_MANDLOCK}},
        {"norelatime", {true, MS_RELATIME}},
        {"nostrictatime", {true, MS_STRICTATIME}},
        {"nosuid", {false, MS_NOSUID}},
        // {"nosymfollow",{false, MS_NOSYMFOLLOW}}, // since kernel 5.10
        {"rbind", {false, MS_BIND | MS_REC}},
        {"relatime", {false, MS_RELATIME}},
        {"remount", {false, MS_REMOUNT}},
        {"ro", {false, MS_RDONLY}},
        {"rw", {true, MS_RDONLY}},
        {"silent", {false, MS_SILENT}},
        {"strictatime", {false, MS_STRICTATIME}},
        {"suid", {true, MS_NOSUID}},
        {"sync", {false, MS_SYNCHRONOUS}},
        // {"symfollow",{true, MS_NOSYMFOLLOW}}, // since kernel 5.10
    };

    o.destination = j.at("destination").get<std::string>();
    o.type = j.at("type").get<std::string>();
    o.fsType = fsTypes.find(o.type)->second;
    if (o.fsType == Mount::kBind) {
        o.flags = MS_BIND;
    }
    o.source = j.at("source").get<std::string>();
    o.data = {};

    // Parse options to data and flags.
    // FIXME: support "propagation flags" and "recursive mount attrs"
    // https://github.com/opencontainers/runc/blob/c83abc503de7e8b3017276e92e7510064eee02a8/libcontainer/specconv/spec_linux.go#L958
    auto options = j.value("options", util::strVec());
    for (auto const &opt : options) {
        auto it = optionFlags.find(opt);
        if (it != optionFlags.end()) {
            if (it->second.clear) {
                o.flags &= ~it->second.flag;
            } else
                o.flags |= it->second.flag;
        } else {
            o.data.push_back(opt);
        }
    }
}

inline void to_json(nlohmann::json &j, const Mount &o)
{
    j["destination"] = o.destination;
    j["source"] = o.source;
    j["type"] = o.type;
    j["options"] = o.data; // FIXME: this data is not original options, some of them have been prased to flags.
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
    uint64_t containerId = 0u;
    uint64_t hostId = 0u;
    uint64_t size = 0u;
};

inline void from_json(const nlohmann::json &j, IDMap &o)
{
    o.hostId = j.value("hostId", 0);
    o.containerId = j.value("containerId", 0);
    o.size = j.value("size", 0);
}

inline void to_json(nlohmann::json &j, const IDMap &o)
{
    j["hostId"] = o.hostId;
    j["containerId"] = o.containerId;
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
    util::strVec names;
    SeccompAction action;
    std::vector<SyscallArg> args;
};

inline void from_json(const nlohmann::json &j, Syscall &o)
{
    o.names = j.at("names").get<util::strVec>();
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
        "preStart": [
            {
                "path": "/usr/bin/fix-mounts",
                "args": ["fix-mounts", "arg1", "arg2"],
                "env":  [ "key1=value1"]
            },
            {
                "path": "/usr/bin/setup-network"
            }
        ],
        "postStart": [
            {
                "path": "/usr/bin/notify-start",
                "timeout": 5
            }
        ],
        "postStop": [
            {
                "path": "/usr/sbin/cleanup.sh",
                "args": ["cleanup.sh", "-f"]
            }
        ]
    }
 */

struct Hook {
    std::string path;
    tl::optional<util::strVec> args;
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
    tl::optional<std::vector<Hook>> preStart;
    tl::optional<std::vector<Hook>> postStart;
    tl::optional<std::vector<Hook>> postStop;
};

inline void from_json(const nlohmann::json &j, Hooks &o)
{
    LLJS_FROM_OPT(preStart);
    LLJS_FROM_OPT(postStart);
    LLJS_FROM_OPT(postStop);
}

inline void to_json(nlohmann::json &j, const Hooks &o)
{
    j["postStop"] = o.postStop;
    j["postStart"] = o.postStart;
    j["preStart"] = o.preStart;
}

struct AnnotationsOverlayfs {
    std::string lowerParent;
    std::string upper;
    std::string workdir;
    std::vector<Mount> mounts;
};

LLJS_FROM_OBJ(AnnotationsOverlayfs)
{
    LLJS_FROM_VARNAME(lowerParent, lowerParent);
    LLJS_FROM(upper);
    LLJS_FROM(workdir);
    LLJS_FROM(mounts);
}

LLJS_TO_OBJ(AnnotationsOverlayfs)
{
    LLJS_TO_VARNAME(lowerParent, lowerParent);
    LLJS_TO(upper);
    LLJS_TO(workdir);
    LLJS_TO(mounts);
}

struct AnnotationsNativeRootfs {
    std::vector<Mount> mounts;
};

LLJS_FROM_OBJ(AnnotationsNativeRootfs)
{
    LLJS_FROM(mounts);
}

LLJS_TO_OBJ(AnnotationsNativeRootfs)
{
    LLJS_TO(mounts);
}

struct DbusProxyInfo {
    bool enable;
    std::string busType;
    std::string appId;
    std::string proxyPath;
    std::vector<std::string> name;
    std::vector<std::string> path;
    std::vector<std::string> interface;
};

LLJS_FROM_OBJ(DbusProxyInfo)
{
    LLJS_FROM(enable);
    LLJS_FROM_VARNAME(busType, busType);
    LLJS_FROM_VARNAME(appId, appId);
    LLJS_FROM_VARNAME(proxyPath, proxyPath);
    LLJS_FROM(name);
    LLJS_FROM(path);
    LLJS_FROM(interface);
}

LLJS_TO_OBJ(DbusProxyInfo)
{
    LLJS_TO(enable);
    LLJS_TO_VARNAME(busType, busType);
    LLJS_TO_VARNAME(appId, appId);
    LLJS_TO_VARNAME(proxyPath, proxyPath);
    LLJS_TO(name);
    LLJS_TO(path);
    LLJS_TO(interface);
}

struct Annotations {
    std::string containerRootPath;
    tl::optional<AnnotationsOverlayfs> overlayfs;
    tl::optional<AnnotationsNativeRootfs> native;
    tl::optional<DbusProxyInfo> dbusProxyInfo;
};

LLJS_FROM_OBJ(Annotations)
{
    LLJS_FROM_VARNAME(containerRootPath, containerRootPath);
    LLJS_FROM_OPT(overlayfs);
    LLJS_FROM_OPT(native);
    LLJS_FROM_OPT_VARNAME(dbusProxyInfo, dbusProxyInfo);
}

LLJS_TO_OBJ(Annotations)
{
    LLJS_TO_VARNAME(containerRootPath, containerRootPath);
    LLJS_TO(overlayfs);
    LLJS_TO(native);
    LLJS_TO_VARNAME(dbusProxyInfo, dbusProxyInfo);
}

struct Runtime {
    std::string version;
    Root root;
    Process process;
    std::string hostname;
    Linux linux;
    tl::optional<std::vector<Mount>> mounts;
    tl::optional<Hooks> hooks;
    tl::optional<Annotations> annotations;
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
    LLJS_FROM_OPT(annotations);
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
    LLJS_TO(annotations);
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

#endif /* LINGLONG_BOX_SRC_UTIL_OCI_RUNTIME_H_ */
