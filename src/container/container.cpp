/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "container.h"

#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include <grp.h>
#include <sched.h>
#include <unistd.h>

#include <cerrno>
#include <utility>
#include <util/debug/debug.h>
#include <util/platform.h>

#include "util/logger.h"
#include "util/filesystem.h"
#include "util/semaphore.h"

#include "seccomp.h"
#include "container_option.h"

namespace linglong {

struct Container::ContainerPrivate {
public:
    ContainerPrivate(Runtime r, Container * /*parent*/)
        : hostRoot(r.root.path)
        , r(std::move(r))
    {
        if (!util::fs::is_dir(hostRoot) || !util::fs::exists(hostRoot)) {
            throw std::runtime_error((hostRoot + " not exist or not a dir").c_str());
        }
    }

    int semID = -1;

    std::string hostRoot;

    Runtime r;

    bool delayNewUserNS = false;
    bool newCgroupNS = false;

    Option opt;

    uid_t hostUid = -1;
    gid_t hostGid = -1;

public:
    //    int dropToUser(int uid, int gid)
    //    {
    //        setuid(uid);
    //        seteuid(uid);
    //        setgid(gid);
    //        setegid(gid);
    //        return 0;
    //    }

    static int dropPermissions()
    {
        __gid_t newgid[1] = {getgid()};
        auto newuid = getuid();
        auto olduid = geteuid();

        if (0 == olduid) {
            setgroups(1, newgid);
        }
        seteuid(newuid);
        return 0;
    }

    int preLinks() const
    {
        chdir("/");

        if (opt.linkLFS) {
            symlink("/usr/bin", "/bin");
            symlink("/usr/lib", "/lib");
            symlink("/usr/lib32", "/lib32");
            symlink("/usr/lib64", "/lib64");
            symlink("/usr/libx32", "/libx32");
        }

        // default link
        symlink("/proc/kcore", "/dev/core");
        symlink("/proc/self/fd", "/dev/fd");
        symlink("/proc/self/fd/2", "/dev/stderr");
        symlink("/proc/self/fd/0", "/dev/stdin");
        symlink("/proc/self/fd/1", "/dev/stdout");
        return 0;
    }

    int preDefaultDevices() const
    {
        struct Device {
            std::string path;
            mode_t mode;
            dev_t dev;
        };

        std::vector<Device> list = {
            {"/dev/null", S_IFCHR | 0666, makedev(1, 3)},    {"/dev/zero", S_IFCHR | 0666, makedev(1, 5)},
            {"/dev/full", S_IFCHR | 0666, makedev(1, 7)},    {"/dev/random", S_IFCHR | 0666, makedev(1, 8)},
            {"/dev/urandom", S_IFCHR | 0666, makedev(1, 9)}, {"/dev/tty", S_IFCHR | 0666, makedev(5, 0)},
        };

        // TODO: not work now
        if (delayNewUserNS && false) {
            for (auto const &dev : list) {
                auto path = hostRoot + dev.path;
                int ret = mknod(path.c_str(), dev.mode, dev.dev);
                if (0 != ret) {
                    logErr() << "mknod" << path << dev.mode << dev.dev << "failed with" << ret << errno;
                }
                chmod(path.c_str(), dev.mode | 0xFFFF);
                chown(path.c_str(), 0, 0);
            }
        } else {
            for (auto const &dev : list) {
                Mount m;
                m.destination = dev.path;
                m.type = "bind";
                m.options = std::vector<std::string> {"bind"};
                m.fsType = Mount::Bind;
                m.source = dev.path;
                this->mountNode(m);
            }
        }

        // FIXME: /dev/console is set up if terminal is enabled in the config by bind mounting the pseudoterminal slave
        // to /dev/console.
        symlink("/dev/ptmx", "/dev/pts/ptmx");
        return 0;
    }

    int pivotRoot() const
    {
        chdir(hostRoot.c_str());

        auto llHost = hostRoot + "/ll-host";
        mkdir(llHost.c_str(), 0777);
        long ret = syscall(SYS_pivot_root, hostRoot.c_str(), "ll-host");
        if (ret != EXIT_SUCCESS) {
            logErr() << "SYS_pivot_root failed" << hostRoot << util::errnoString() << errno << ret;
            return EXIT_FAILURE;
        }
        chdir("/");

        ret = chroot(".");
        if (ret != EXIT_SUCCESS) {
            logErr() << "chroot failed" << hostRoot << util::errnoString() << errno;
            return EXIT_FAILURE;
        }
        umount2("/ll-host", MNT_DETACH);
        return 0;
    }

    int mountNode(Mount m) const
    {
        int ret = 0;
        struct stat sourceStat {
        };
        __mode_t destMode = 0755;
        // FIXME(use what permission???);
        __mode_t destParentMode = 0755;

        ret = lstat(m.source.c_str(), &sourceStat);

        if (0 == ret) {
            destMode = sourceStat.st_mode & 0xFFFF;
        } else {
            // source not exist
            if (m.fsType == Mount::Bind) {
                logErr() << "lstat" << m.source << "failed";
                return -1;
            }
        }

        auto destFullPath = this->hostRoot + m.destination;
        // FIXME: should be no swap root
        if (opt.rootless) {
            destFullPath = m.destination;
        }

        auto destParentPath = util::fs::path(destFullPath).parent_path();

        switch (sourceStat.st_mode & S_IFMT) {
        case S_IFCHR: {
            util::fs::create_directories(destParentPath, destParentMode);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFSOCK: {
            // FIXME: can not mound dbus socket on rootless
            util::fs::create_directories(destParentPath, destParentMode);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFLNK: {
            util::fs::create_directories(destParentPath, destParentMode);
            std::ofstream output(destFullPath);
            m.source = util::fs::read_symlink(util::fs::path(m.source)).string();
            break;
        }
        case S_IFREG: {
            util::fs::create_directories(destParentPath, destParentMode);
            std::ofstream output(destFullPath);
            break;
        }
        case S_IFDIR:
            util::fs::create_directories(util::fs::path(destFullPath), destMode);
            break;
        default:
            logWan() << "unknown file type" << (sourceStat.st_mode & S_IFMT) << m.source;
            // FIXME: show error
            util::fs::create_directories(util::fs::path(destFullPath), destMode);
            break;
        }

        for (const auto &option : m.options) {
            if (option.rfind("mode=", 0) == 0) {
                auto equalPos = option.find('=');
                auto mode = option.substr(equalPos + 1, option.length() - equalPos - 1);
                chmod(destFullPath.c_str(), std::stoi(mode, nullptr, 8));
            } else {
            }
        }

        uint32_t flags = m.flags;
        auto opts = util::str_vec_join(m.options, ',');

        //        logDbg() << "mount" << m.source << "to" << destFullPath << flags << opts;

        switch (m.fsType) {
        case Mount::Bind:
            // FIXME: maybe not work if in user namespace
            //            chmod(destFullPath.c_str(), sourceStat.st_mode);
            //            chown(destFullPath.c_str(), sourceStat.st_uid, sourceStat.st_gid);
            ret = ::mount(m.source.c_str(), destFullPath.c_str(), nullptr, MS_BIND, nullptr);
            if (0 != ret) {
                logErr() << "mount" << m.source << "failed" << util::errnoString() << errno;
                break;
            }
            ret = ::mount(m.source.c_str(), destFullPath.c_str(), nullptr, flags | MS_BIND | MS_REMOUNT, opts.c_str());
            if (0 != ret) {
                logErr() << "remount" << m.source << "failed" << util::errnoString() << errno;
                break;
            }
            break;
        case Mount::Proc:
        case Mount::Devpts:
        case Mount::Mqueue:
        case Mount::Tmpfs:
        case Mount::Sysfs:
            ret = ::mount(m.source.c_str(), destFullPath.c_str(), m.type.c_str(), flags, nullptr);
            break;
        case Mount::Cgroup2:
            ret = ::mount(m.source.c_str(), destFullPath.c_str(), m.type.c_str(), flags, opts.c_str());
            break;
        default:
            logErr() << "unsupported type" << m.type;
        }

        if (EXIT_SUCCESS != ret) {
            logErr() << "mount" << m.source << "to" << destFullPath << m.type << flags << opts << util::errnoString()
                     << errno;
        }

        return ret;
    }

    static int configUserNamespace(const linglong::Linux &linux, int initPid)
    {
        std::string pid = "self";
        if (initPid > 0) {
            pid = util::format("%d", initPid);
        }

        logDbg() << "write uid_map and pid_map" << initPid;

        // write uid map
        std::ofstream uidMapFile(util::format("/proc/%s/uid_map", pid.c_str()));
        for (auto const &idMap : linux.uidMappings) {
            uidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
        }
        uidMapFile.close();

        // write gid map
        auto setgroupsPath = util::format("/proc/%s/setgroups", pid.c_str());
        std::ofstream setgroupsFile(setgroupsPath);
        setgroupsFile << "deny";
        setgroupsFile.close();

        std::ofstream gidMapFile(util::format("/proc/%s/gid_map", pid.c_str()));
        for (auto const &idMap : linux.gidMappings) {
            gidMapFile << util::format("%lu %lu %lu\n", idMap.containerID, idMap.hostID, idMap.size);
        }
        gidMapFile.close();

        logDbg() << "finish write uid_map and pid_map";
        return 0;
    }

    static int configCgroupV2(const std::string &cgroupsPath, const linglong::Resources &res, int initPid)
    {
        if (cgroupsPath.empty()) {
            return 0;
        }

        auto writeConfig = [](const std::map<std::string, std::string> &cfgs) {
            for (auto const &cfg : cfgs) {
                std::ofstream cfgFile(cfg.first);
                cfgFile << cfg.second << std::endl;
                cfgFile.close();
            }
        };

        auto cgroupsRoot = util::fs::path(cgroupsPath);
        util::fs::create_directories(cgroupsRoot, 0755);

        int ret = mount("cgroup2", cgroupsRoot.string().c_str(), "cgroup2", 0u, nullptr);
        if (ret != 0) {
            logErr() << "mount cgroup failed" << util::errnoString();
            return ret;
        }

        // TODO: should sub path with pid?
        auto subCgroupRoot = cgroupsRoot / "ll-box";
        util::fs::create_directories(util::fs::path(subCgroupRoot), 0755);

        auto subCgroupPath = [subCgroupRoot](const std::string &compents) -> std::string {
            return (subCgroupRoot / compents).string();
        };

        // config memory
        const auto memMax = res.memory.limit;

        if (memMax > 0) {
            const auto memSwapMax = res.memory.swap - memMax;
            const auto memLow = res.memory.reservation;
            writeConfig({
                {subCgroupPath("memory.max"), util::format("%d", memMax)},
                {subCgroupPath("memory.swap.max"), util::format("%d", memSwapMax)},
                {subCgroupPath("memory.low"), util::format("%d", memLow)},
            });
        }

        // config cpu
        const auto cpuPeriod = res.cpu.period;
        const auto cpuMax = res.cpu.quota;
        // convert from [2-262144] to [1-10000], 262144 is 2^18
        const auto cpuWeight = (1 + ((res.cpu.shares - 2) * 9999) / 262142);

        writeConfig({
            {subCgroupPath("cpu.max"), util::format("%d %d", cpuMax, cpuPeriod)},
            {subCgroupPath("cpu.weight"), util::format("%d", cpuWeight)},
        });

        // config pid
        writeConfig({
            {subCgroupPath("cgroup.procs"), util::format("%d", initPid)},
        });

        logDbg() << "move" << initPid << "to new cgroups";

        return 0;
    }
};

// int hookExec(const tl::optional<linglong::Hooks> hooks)
int hookExec(const Hook &hook)
{
    int execPid = fork();

    if (execPid < 0) {
        logErr() << "fork failed" << util::retErrString(execPid);
        return -1;
    }

    if (execPid == 0) {
        util::str_vec argStrVec;
        argStrVec.push_back(hook.path);

        std::copy(hook.args->begin(), hook.args->end(), std::back_inserter(argStrVec));

        auto targetArgc = argStrVec.size();
        const char *targetArgv[targetArgc + 1];

        for (decltype(targetArgc) i = 1; i < targetArgc; i++) {
            targetArgv[i] = argStrVec[i].c_str();
        }
        targetArgv[targetArgc] = nullptr;

        auto targetEnvc = hook.env.has_value() ? hook.env->size() : 0;
        const char **targetEnvv = targetEnvc ? new const char *[targetEnvc + 1] : nullptr;
        for (decltype(targetEnvc) i = 0; i < targetEnvc; i++) {
            targetEnvv[i] = hook.env->at(i).c_str();
        }
        if (targetEnvv) {
            targetEnvv[targetEnvc] = nullptr;
        }

        logDbg() << "hookExec" << targetArgv[0] << targetEnvv << "@"
                 << "with pid:" << getpid();

        int ret = execve(targetArgv[0], const_cast<char **>(targetArgv), const_cast<char **>(targetEnvv));
        if (0 != ret) {
            logErr() << "execve failed" << util::retErrString(ret);
        }
        logErr() << "execve return" << util::retErrString(ret);

        delete[] targetEnvv;
        return ret;
    }

    return waitpid(execPid, 0, 0);
}

int noPrivilegeProc(void *arg)
{
    auto &c = *reinterpret_cast<Container::ContainerPrivate *>(arg);

    //    dumpIDMap();
    //    dumpUidGidGroup();
    //    dumpFilesystem("/");

    if (c.opt.rootless) {
        logDbg() << "wait switch to normal user" << c.semID;
        Semaphore noPrivilegeSem(c.semID);
        noPrivilegeSem.vrijgeven();
        noPrivilegeSem.passeren();
        logDbg() << "wait switch to normal user end";
    }

    if (c.r.hooks.has_value() && c.r.hooks->prestart.has_value()) {
        for (auto const &preStart : *c.r.hooks->prestart) {
            hookExec(preStart);
        }
    }

    if (!c.opt.rootless) {
        seteuid(0);
        // todo: check return value
        configSeccomp(c.r.linux.seccomp);
        Container::ContainerPrivate::dropPermissions();
    }

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    logDbg() << "c.r.process.args:" << c.r.process.args;

    auto targetArgc = c.r.process.args.size();
    auto targetArgv = new const char *[targetArgc + 1];
    std::vector<std::string> argvStringList;
    for (size_t i = 0; i < targetArgc; i++) {
        argvStringList.push_back(c.r.process.args[i]);
        targetArgv[i] = (char *)(argvStringList[i].c_str());
    }
    targetArgv[targetArgc] = nullptr;
    char *const *execArgv = const_cast<char **>(targetArgv);

    auto targetEnvCount = c.r.process.env.size();
    auto targetEnvList = new const char *[targetEnvCount + 1];
    for (size_t i = 0; i < targetEnvCount; i++) {
        targetEnvList[i] = (char *)(c.r.process.env[i].c_str());
    }
    targetEnvList[targetEnvCount] = nullptr;
    char *const *execEnvList = const_cast<char **>(targetEnvList);

    logDbg() << "execve" << targetArgv[0] << execArgv[0] << execEnvList << "@" << c.r.process.cwd
             << " with pid:" << getpid();

    auto ret = execve(targetArgv[0], execArgv, execEnvList);
    if (0 != ret) {
        logErr() << "execve failed" << util::retErrString(ret);
    }
    logDbg() << "execve return" << util::retErrString(ret);
    delete[] targetArgv;
    delete[] targetEnvList;
    return ret;
}

int entryProc(void *arg)
{
    auto &c = *reinterpret_cast<Container::ContainerPrivate *>(arg);

    Semaphore s(c.semID);

    if (c.opt.rootless) {
        s.vrijgeven();
        s.passeren();
    }

    // FIXME: change HOSTNAME will broken XAUTH
    auto newHostname = c.r.hostname;
    //    if (sethostname(newHostname.c_str(), strlen(newHostname.c_str())) == -1) {
    //        logErr() << "sethostname failed" << util::errnoString();
    //        return -1;
    //    }

    uint32_t flags = MS_REC | MS_SLAVE;
    int ret = mount(nullptr, "/", nullptr, flags, nullptr);
    if (0 != ret) {
        logErr() << "mount / failed" << util::retErrString(ret);
        return -1;
    }

    ret = mount("tmpfs", c.hostRoot.c_str(), "tmpfs", MS_NODEV | MS_NOSUID, nullptr);
    if (0 != ret) {
        logErr() << "mount" << c.hostRoot << "failed" << util::retErrString(ret);
        return -1;
    }

    chdir(c.hostRoot.c_str());

    if (c.r.mounts.has_value()) {
        for (auto const &m : c.r.mounts.value()) {
            c.mountNode(m);
        }
    }
    c.preDefaultDevices();

    c.configCgroupV2(c.r.linux.cgroupsPath, c.r.linux.resources, getpid());

    if (!c.opt.rootless) {
        c.pivotRoot();
        c.preLinks();

        auto unshareFlags = 0;
        if (c.delayNewUserNS) {
            unshareFlags |= CLONE_NEWUSER;
        }
        if (c.newCgroupNS) {
            unshareFlags |= CLONE_NEWCGROUP;
        }
        if (unshareFlags) {
            ret = unshare(unshareFlags);
            if (0 != ret) {
                logErr() << "unshare failed" << unshareFlags << util::retErrString(ret);
            }
        }
        s.vrijgeven();
        s.passeren();
    }

    chdir(c.r.process.cwd.c_str());

    c.semID = getpid();
    Semaphore noPrivilegeSem(c.semID);
    noPrivilegeSem.init();

    int noPrivilegeProcFlag = SIGCHLD | CLONE_NEWNS;
    if (c.opt.rootless) {
        noPrivilegeProcFlag |= CLONE_NEWUSER;
    }

    int noPrivilegePid = platformClone(noPrivilegeProc, noPrivilegeProcFlag, arg);

    if (noPrivilegePid < 0) {
        logErr() << "clone failed" << util::retErrString(noPrivilegePid);
        return EXIT_FAILURE;
    }

    if (c.opt.rootless) {
        noPrivilegeSem.passeren();

        Linux linux;
        IDMap idMap;

        idMap.containerID = c.hostUid;
        idMap.hostID = 0;
        idMap.size = 1;
        linux.uidMappings.push_back(idMap);

        idMap.containerID = c.hostGid;
        idMap.hostID = 0;
        idMap.size = 1;
        linux.gidMappings.push_back(idMap);

        logDbg() << "switch to normal user" << noPrivilegePid;
        Container::ContainerPrivate::configUserNamespace(linux, noPrivilegePid);

        noPrivilegeSem.vrijgeven();
        logDbg() << "switch to normal end" << noPrivilegePid;
    }

    Container::ContainerPrivate::dropPermissions();

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    while (true) {
        int childPid = waitpid(-1, nullptr, 0);
        if (childPid > 0) {
            logDbg() << "wait" << childPid << "exit";
        }
        if (childPid < 0) {
            logDbg() << "all child exit" << childPid;
            return 0;
        }
    }
}

Container::Container(const Runtime &r)
    : dd_ptr(new ContainerPrivate(r, this))
{
}

int Container::start(const Option &opt)
{
    auto &c = *reinterpret_cast<Container::ContainerPrivate *>(dd_ptr.get());
    c.opt = opt;

    if (opt.rootless) {
        c.hostUid = geteuid();
        c.hostGid = getegid();
    }

    int flags = SIGCHLD | CLONE_NEWNS;

    for (auto const &n : dd_ptr->r.linux.namespaces) {
        switch (n.type) {
        case CLONE_NEWIPC:
        case CLONE_NEWUTS:
        case CLONE_NEWNS:
        case CLONE_NEWPID:
        case CLONE_NEWNET:
            flags |= n.type;
            break;
        case CLONE_NEWUSER:
            dd_ptr->delayNewUserNS = true;
            break;
        case CLONE_NEWCGROUP:
            dd_ptr->newCgroupNS = true;
            break;
        default:
            return -1;
        }
    }

    if (opt.rootless) {
        flags |= CLONE_NEWUSER;
    }

    c.semID = getpid();
    Semaphore sem(c.semID);
    sem.init();

    int entryPid = platformClone(entryProc, flags, (void *)dd_ptr.get());
    if (entryPid <= -1) {
        logErr() << "clone failed" << util::retErrString(entryPid);
        return -1;
    }

    if (c.delayNewUserNS || opt.rootless) {
        logDbg() << "wait child start" << entryPid;
        sem.passeren();
        ContainerPrivate::configUserNamespace(c.r.linux, entryPid);
        sem.vrijgeven();
        logDbg() << "wait child end";
    }

    ContainerPrivate::dropPermissions();

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    if (waitpid(-1, nullptr, 0) < 0) {
        logErr() << "waitpid failed" << util::errnoString();
        return -1;
    }
    logInf() << "wait" << entryPid << "finish";

    return 0;
}

Container::~Container() = default;

} // namespace linglong