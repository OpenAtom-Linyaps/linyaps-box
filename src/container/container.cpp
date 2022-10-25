/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
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
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <grp.h>
#include <sched.h>
#include <unistd.h>

#include <cerrno>
#include <map>
#include <utility>

#include "util/logger.h"
#include "util/filesystem.h"
#include "util/semaphore.h"
#include "util/debug/debug.h"
#include "util/platform.h"

#include "container/seccomp.h"
#include "container/container_option.h"
#include "container/mount/host_mount.h"
#include "container/mount/filesystem_driver.h"

namespace linglong {

static const std::string LingLongDbusProxyBin = "/usr/bin/ll-dbus-proxy";
// start dbus proxy
static int startDbusProxy(const Runtime &runtime)
{
    if (!(runtime.annotations.has_value() && runtime.annotations->dbusProxyInfo.has_value()
          && runtime.annotations->dbusProxyInfo->enable)) {
        logInf() << "dbus proxy disabled";
        return -1;
    }

    const auto &info = runtime.annotations->dbusProxyInfo;

    std::string socketPath = info->proxyPath;

    pid_t proxyPid = fork();
    if (proxyPid < 0) {
        logErr() << "fork to start dbus proxy failed:", util::errnoString();
        return -1;
    }

    if (0 == proxyPid) {
        // FIXME: parent may dead before this return.
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        std::string busType = info->busType;
        std::string appId = info->appId;
        std::vector<std::string> nameFliter = info->name;
        std::vector<std::string> pathFliter = info->path;
        std::vector<std::string> interfaceFliter = info->interface;
        std::string nameFliterString = linglong::util::strVecJoin(nameFliter, ',');
        std::string pathFliterString = linglong::util::strVecJoin(pathFliter, ',');
        std::string interfaceFliterString = linglong::util::strVecJoin(interfaceFliter, ',');
        char const *const args[] = {LingLongDbusProxyBin.c_str(),
                                    appId.c_str(),
                                    busType.c_str(),
                                    socketPath.c_str(),
                                    nameFliterString.c_str(),
                                    pathFliterString.c_str(),
                                    interfaceFliterString.c_str(),
                                    NULL};
        int ret = execvp(args[0], (char **)args);
        logErr() << "start dbus proxy failed, ret=" << ret;
        exit(ret);
    } else {
        // FIXME: this call make 10ms lag at least
        if (util::fs::Path(socketPath).waitUntilExsit() != 0) {
            logErr() << util::format("timeout! socketPath [\"%s\"] not exsit", socketPath.c_str());
            return -1;
        }
    }
    return 0;
}

int dropToNormalUser(int uid, int gid)
{
    setuid(uid);
    seteuid(uid);
    setgid(gid);
    setegid(gid);
    return 0;
}

static int configUserNamespace(const linglong::Linux &linux, int initPid)
{
    std::string pid = "self";
    if (initPid > 0) {
        pid = util::format("%d", initPid);
    }

    logDbg() << "old uid:" << getuid() << "gid:" << getgid();
    logDbg() << "start write uid_map and pid_map" << initPid;

    // write uid map
    std::ofstream uidMapFile(util::format("/proc/%s/uid_map", pid.c_str()));
    for (auto const &idMap : linux.uidMappings) {
        uidMapFile << util::format("%lu %lu %lu\n", idMap.containerId, idMap.hostId, idMap.size);
    }
    uidMapFile.close();

    // write gid map
    auto setgroupsPath = util::format("/proc/%s/setgroups", pid.c_str());
    std::ofstream setgroupsFile(setgroupsPath);
    setgroupsFile << "deny";
    setgroupsFile.close();

    std::ofstream gidMapFile(util::format("/proc/%s/gid_map", pid.c_str()));
    for (auto const &idMap : linux.gidMappings) {
        gidMapFile << util::format("%lu %lu %lu\n", idMap.containerId, idMap.hostId, idMap.size);
    }
    gidMapFile.close();

    logDbg() << "new uid:" << getuid() << "gid:" << getgid();
    return 0;
}

// FIXME(iceyer): not work now
static int configCgroupV2(const std::string &cgroupsPath, const linglong::Resources &res, int initPid)
{
    if (cgroupsPath.empty()) {
        logWan() << "skip with empty cgroupsPath";
        return 0;
    }

    auto writeConfig = [](const std::map<std::string, std::string> &cfgs) {
        for (auto const &cfg : cfgs) {
            logWan() << "configCgroupV2" << cfg.first << cfg.second;
            std::ofstream cfgFile(cfg.first);
            cfgFile << cfg.second << std::endl;
            cfgFile.close();
        }
    };

    auto cgroupsRoot = util::fs::Path(cgroupsPath);
    util::fs::createDirectories(cgroupsRoot, 0755);

    int ret = mount("cgroup2", cgroupsRoot.string().c_str(), "cgroup2", 0u, nullptr);
    if (ret != 0) {
        logErr() << "mount cgroup failed" << util::retErrString(ret);
        return -1;
    }

    // TODO: should sub Path with pid?
    auto subCgroupRoot = cgroupsRoot / "ll-box";
    if (!util::fs::createDirectories(util::fs::Path(subCgroupRoot), 0755)) {
        logErr() << "createDirectories subCgroupRoot failed" << util::errnoString();
        return -1;
    }

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

inline void epollCtlAdd(int epfd, int fd)
{
    static epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0)
        logWan() << util::errnoString();
}

// if wstatus says child exit normally, return true else false
static bool parseWstatus(const int &wstatus, std::string &info)
{
    if (WIFEXITED(wstatus)) {
        auto code = WEXITSTATUS(wstatus);
        info = util::format("exited with code %d", code);
        return code == 0;
    } else if (WIFSIGNALED(wstatus)) {
        info = util::format("terminated by signal %d", WTERMSIG(wstatus));
        return false;
    } else {
        info = util::format("is dead with wstatus=%d", wstatus);
        return false;
    }
}

struct ContainerPrivate {
public:
    ContainerPrivate(Runtime runtime, std::unique_ptr<util::MessageReader> reader, Container * /*parent*/)
        : hostRoot(runtime.root.path)
        , runtime(std::move(runtime))
        , nativeMounter(new HostMount)
        , overlayfsMounter(new HostMount)
        , fuseProxyMounter(new HostMount)
        , reader(std::move(reader))
    {
    }

    std::string hostRoot;

    Runtime runtime;

    bool useNewCgroupNs = false;

    Option opt;

    uid_t hostUid = -1;
    gid_t hostGid = -1;

    std::unique_ptr<HostMount> nativeMounter;
    std::unique_ptr<HostMount> overlayfsMounter;
    std::unique_ptr<HostMount> fuseProxyMounter;

    HostMount *containerMounter = nullptr;

    std::unique_ptr<util::MessageReader> reader;

    std::map<int, std::string> pidMap;

public:
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

    int prepareLinks() const
    {
        chdir("/");

        if (opt.linkLfs) {
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

    int prepareDefaultDevices() const
    {
        struct Device {
            std::string Path;
            mode_t mode;
            dev_t dev;
        };

        std::vector<Device> list = {
            {"/dev/null", S_IFCHR | 0666, makedev(1, 3)},    {"/dev/zero", S_IFCHR | 0666, makedev(1, 5)},
            {"/dev/full", S_IFCHR | 0666, makedev(1, 7)},    {"/dev/random", S_IFCHR | 0666, makedev(1, 8)},
            {"/dev/urandom", S_IFCHR | 0666, makedev(1, 9)}, {"/dev/tty", S_IFCHR | 0666, makedev(5, 0)},
        };

        // TODO(iceyer): not work now
        if (!opt.rootLess) {
            for (auto const &dev : list) {
                auto Path = hostRoot + dev.Path;
                int ret = mknod(Path.c_str(), dev.mode, dev.dev);
                if (0 != ret) {
                    logErr() << "mknod" << Path << dev.mode << dev.dev << "failed with" << util::retErrString(ret);
                }
                chmod(Path.c_str(), dev.mode | 0xFFFF);
                chown(Path.c_str(), 0, 0);
            }
        } else {
            for (auto const &dev : list) {
                Mount m;
                m.destination = dev.Path;
                m.type = "bind";
                m.data = std::vector<std::string> {};
                m.flags = MS_BIND;
                m.fsType = Mount::kBind;
                m.source = dev.Path;

                this->containerMounter->mountNode(m);
            }
        }

        // FIXME(iceyer): /dev/console is set up if terminal is enabled in the config by bind mounting the
        // pseudoterminal slave to /dev/console.
        symlink("/dev/pts/ptmx", (this->hostRoot + "/dev/ptmx").c_str());

        return 0;
    }

    void waitChildAndExec()
    {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGTERM);
        // FIXME: add more signals.

        /* Block signals so that they aren't handled
           according to their default dispositions. */

        if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
            logWan() << "sigprocmask";

        int sfd = signalfd(-1, &mask, 0);
        if (sfd == -1)
            logWan() << "signalfd";

        auto epfd = epoll_create(1);
        epollCtlAdd(epfd, sfd);
        epollCtlAdd(epfd, reader->fd);

        for (;;) {
            struct epoll_event events[10];
            int event_cnt = epoll_wait(epfd, events, 5, -1);
            for (int i = 0; i < event_cnt; i++) {
                const auto &event = events[i];
                if (event.data.fd == sfd) {
                    struct signalfd_siginfo fdsi;
                    ssize_t s = read(sfd, &fdsi, sizeof(fdsi));
                    if (s != sizeof(fdsi)) {
                        logWan() << "error read from signal fd";
                    }
                    if (fdsi.ssi_signo == SIGCHLD) {
                        logWan() << "recive SIGCHLD";

                        int wstatus;
                        while (int child = waitpid(-1, &wstatus, WNOHANG)) {
                            if (child > 0) {
                                std::string info;
                                auto normal = parseWstatus(wstatus, info);
                                info = util::format("child [%d] [%s].", child, info.c_str());
                                if (normal) {
                                    logDbg() << info;
                                } else {
                                    logWan() << info;
                                }
                                auto it = pidMap.find(child);
                                if (it != pidMap.end()) {
                                    reader->writeChildExit(child, it->second, wstatus, info);
                                    pidMap.erase(it);
                                }
                            } else if (child < 0) {
                                if (errno == ECHILD) {
                                    logDbg() << util::format("no child to wait");
                                    return;
                                } else {
                                    auto str = util::errnoString();
                                    logErr() << util::format("waitpid failed, %s", str.c_str());
                                    return;
                                }
                            }
                        }
                    } else if (fdsi.ssi_signo == SIGTERM) {
                        // FIXME: box should exit with failed return code.
                        logWan() << util::format("Terminated\n");
                        return;
                    } else {
                        logWan() << util::format("Read unexpected signal [%d]\n", fdsi.ssi_signo);
                    }
                } else if (event.data.fd == reader->fd) {
                    auto json = reader->read();
                    if (json.empty()) {
                        break;
                    }
                    auto p = json.get<Process>();
                    forkAndExecProcess(p);
                } else {
                    logWan() << "kUnknown fd";
                }
            }
        }
        return;
    }

    bool forkAndExecProcess(const Process p)
    {
        // FIXME: parent may dead before this return.
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        int pid = fork();
        if (pid < 0) {
            logErr() << "fork failed" << util::retErrString(pid);
            return false;
        }

        if (0 == pid) {
            logDbg() << "process.args:" << p.args;

            int ret;
            ret = chdir(p.cwd.c_str());
            if (ret) {
                logErr() << "failed to chdir to" << p.cwd.c_str();
            }

            // for PATH
            for (auto const &env : p.env) {
                auto kv = util::strSpilt(env, "=");
                if (kv.size() == 2)
                    setenv(kv.at(0).c_str(), kv.at(1).c_str(), 1);
                else if (kv.size() == 1) {
                    setenv(kv.at(0).c_str(), "", 1);
                } else {
                    logWan() << "kUnknown env:" << env;
                }
            }

            logInf() << "start exec process";
            ret = util::exec(p.args, p.env);
            if (0 != ret) {
                logErr() << "exec failed" << util::retErrString(ret);
            }
            exit(ret);
        } else {
            pidMap.insert(make_pair(pid, p.args[0]));
        }

        return true;
    }

    int pivotRoot() const
    {
        int ret = -1;
        chdir(hostRoot.c_str());

        if (opt.rootLess && runtime.annotations->overlayfs.has_value()) {
            int flag = MS_MOVE;
            ret = mount(".", "/", nullptr, flag, nullptr);
            if (0 != ret) {
                logErr() << "mount / failed" << util::retErrString(ret);
                return -1;
            }

            ret = chroot(".");
            if (0 != ret) {
                logErr() << "chroot . failed" << util::retErrString(ret);
                return -1;
            }
        } else {
            int flag = MS_BIND | MS_REC;
            ret = mount(".", ".", "bind", flag, nullptr);
            if (0 != ret) {
                logErr() << "mount / failed" << util::retErrString(ret);
                return -1;
            }

            auto lingLongHostFilename = "ll-host";

            auto lingLongHostPath = hostRoot + "/" + lingLongHostFilename;

            mkdir(lingLongHostPath.c_str(), 0755);

            ret = syscall(SYS_pivot_root, hostRoot.c_str(), lingLongHostPath.c_str());
            if (0 != ret) {
                logErr() << "SYS_pivot_root failed" << hostRoot << util::errnoString() << errno << ret;
                return -1;
            }

            chdir("/");
            ret = chroot(".");
            if (0 != ret) {
                logErr() << "chroot failed" << hostRoot << util::errnoString() << errno;
                return -1;
            }

            chdir("/");
            umount2(lingLongHostFilename, MNT_DETACH);
        }

        return 0;
    }

    int prepareRootfs()
    {
        auto PrepareOverlayfsRootfs = [&](const AnnotationsOverlayfs &overlayfs) -> int {
            nativeMounter->setup(new NativeFilesystemDriver(overlayfs.lowerParent));

            util::strVec lowerDirs = {};
            int prefix_index = 0;
            for (auto m : overlayfs.mounts) {
                auto prefix = util::fs::Path(util::format("/%d", prefix_index));
                m.destination = (prefix / m.destination).string();
                if (0 == nativeMounter->mountNode(m)) {
                    lowerDirs.push_back((util::fs::Path(overlayfs.lowerParent) / prefix).string());
                }
                ++prefix_index;
            }

            overlayfsMounter->setup(
                new OverlayfsFuseFilesystemDriver(lowerDirs, overlayfs.upper, overlayfs.workdir, hostRoot));

            containerMounter = overlayfsMounter.get();
            return -1;
        };

        auto PrepareFuseProxyRootfs = [&](const AnnotationsOverlayfs &overlayfs) -> int {
            nativeMounter->setup(new NativeFilesystemDriver(overlayfs.lowerParent));

            util::strVec mounts = {};
            for (auto const &m : overlayfs.mounts) {
                auto mount_item = util::format("%s:%s\n", m.source.c_str(), m.destination.c_str());
                mounts.push_back(mount_item);
            }

            fuseProxyMounter->setup(new FuseProxyFilesystemDriver(mounts, hostRoot));

            containerMounter = fuseProxyMounter.get();
            return -1;
        };

        auto PrepareNativeRootfs = [&](const AnnotationsNativeRootfs &native) -> int {
            nativeMounter->setup(new NativeFilesystemDriver(runtime.root.path));

            for (auto m : native.mounts) {
                nativeMounter->mountNode(m);
            }

            containerMounter = nativeMounter.get();
            return -1;
        };

        if (runtime.annotations.has_value() && runtime.annotations->overlayfs.has_value()) {
            auto env = getenv("LL_BOX_FS_BACKEND");
            if (env && "fuse-proxy" == std::string(env)) {
                return PrepareFuseProxyRootfs(runtime.annotations->overlayfs.value());
            } else {
                return PrepareOverlayfsRootfs(runtime.annotations->overlayfs.value());
            }
        } else {
            return PrepareNativeRootfs(runtime.annotations->native.value());
        }

        return -1;
    }

    int mountContainerPath()
    {
        if (runtime.mounts.has_value()) {
            for (auto const &m : runtime.mounts.value()) {
                containerMounter->mountNode(m);
            }
        };

        return 0;
    }
};

int hookExec(const Hook &hook)
{
    int execPid = fork();
    if (execPid < 0) {
        logErr() << "fork failed" << util::retErrString(execPid);
        return -1;
    }

    if (0 == execPid) {
        util::strVec argStrVec;
        argStrVec.push_back(hook.path);

        std::copy(hook.args->begin(), hook.args->end(), std::back_inserter(argStrVec));

        util::exec(argStrVec, hook.env.value());

        exit(0);
    }

    return waitpid(execPid, nullptr, 0);
}

int nonePrivilegeProc(void *arg)
{
    auto &container = *reinterpret_cast<ContainerPrivate *>(arg);

    if (container.opt.rootLess) {
        // TODO(iceyer): use option

        Linux linux;
        IDMap IdMap;

        IdMap.containerId = container.hostUid;
        IdMap.hostId = 0;
        IdMap.size = 1;
        linux.uidMappings.push_back(IdMap);

        IdMap.containerId = container.hostGid;
        IdMap.hostId = 0;
        IdMap.size = 1;
        linux.gidMappings.push_back(IdMap);

        configUserNamespace(linux, 0);
    }

    auto ret = mount("proc", "/proc", "proc", 0, nullptr);
    if (0 != ret) {
        logErr() << "mount proc failed" << util::retErrString(ret);
        return -1;
    }

    if (container.runtime.hooks.has_value() && container.runtime.hooks->preStart.has_value()) {
        for (auto const &preStart : *container.runtime.hooks->preStart) {
            hookExec(preStart);
        }
    }

    if (!container.opt.rootLess) {
        seteuid(0);
        // todo: check return value
        configSeccomp(container.runtime.linux.seccomp);
        ContainerPrivate::dropPermissions();
    }

    container.forkAndExecProcess(container.runtime.process);

    container.waitChildAndExec();
    return 0;
}

void sigtermHandler(int)
{
    exit(-1);
}

int entryProc(void *arg)
{
    auto &container = *reinterpret_cast<ContainerPrivate *>(arg);

    if (container.opt.rootLess) {
        configUserNamespace(container.runtime.linux, 0);
    }

    // FIXME: change HOSTNAME will broken XAUTH
    auto new_hostname = container.runtime.hostname;
    //    if (sethostname(new_hostname.c_str(), strlen(new_hostname.c_str())) == -1) {
    //        logErr() << "sethostname failed" << util::errnoString();
    //        return -1;
    //    }

    uint32_t flags = MS_REC | MS_SLAVE;
    int ret = mount(nullptr, "/", nullptr, flags, nullptr);
    if (0 != ret) {
        logErr() << "mount / failed" << util::retErrString(ret);
        return -1;
    }

    std::string container_root = container.runtime.annotations->containerRootPath;
    flags = MS_NODEV | MS_NOSUID;
    ret = mount("tmpfs", container_root.c_str(), "tmpfs", flags, nullptr);
    if (0 != ret) {
        logErr() << util::format("mount container root (%s) failed:", container_root.c_str())
                 << util::retErrString(ret);
        return -1;
    }

    // NOTE(iceyer): it's not standard oci action
    container.prepareRootfs();

    container.mountContainerPath();

    if (container.useNewCgroupNs) {
        configCgroupV2(container.runtime.linux.cgroupsPath, container.runtime.linux.resources, getpid());
    }

    container.prepareDefaultDevices();

    container.pivotRoot();

    container.prepareLinks();

    if (!container.opt.rootLess) {
        auto unshare_flags = 0;
        // TODO(iceyer): no need user namespace in setuid
        //        if (c.use_delay_new_user_ns) {
        //            unshare_flags |= CLONE_NEWUSER;
        //        }
        if (container.useNewCgroupNs) {
            unshare_flags |= CLONE_NEWCGROUP;
        }
        if (unshare_flags) {
            ret = unshare(unshare_flags);
            if (0 != ret) {
                logErr() << "unshare failed" << unshare_flags << util::retErrString(ret);
            }
        }
        //        if (c.use_delay_new_user_ns) {
        //            s.plusOne();
        //            s.minusOne();
        //        }
    }

    int none_privilege_proc_flag = SIGCHLD | CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNS;

    int noPrivilegePid = util::platformClone(nonePrivilegeProc, none_privilege_proc_flag, arg);
    if (noPrivilegePid < 0) {
        logErr() << "clone failed" << util::retErrString(noPrivilegePid);
        return -1;
    }

    ContainerPrivate::dropPermissions();

    // FIXME: parent may dead before this return.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell

    container.reader.reset();
    signal(SIGTERM, sigtermHandler);
    util::waitAllUntil(noPrivilegePid);
    return -1;
}

Container::Container(const Runtime &r, std::unique_ptr<util::MessageReader> reader)
    : containerPrivate(new ContainerPrivate(r, std::move(reader), this))
{
}

int Container::start(const Option &opt)
{
    auto &container = *reinterpret_cast<ContainerPrivate *>(containerPrivate.get());
    container.opt = opt;

    if (opt.rootLess) {
        container.hostUid = geteuid();
        container.hostGid = getegid();
    }

    int flags = SIGCHLD | CLONE_NEWNS;

    for (auto const &ns : containerPrivate->runtime.linux.namespaces) {
        switch (ns.type) {
        case CLONE_NEWIPC:
        case CLONE_NEWUTS:
        case CLONE_NEWNS:
        case CLONE_NEWPID:
        case CLONE_NEWNET:
            flags |= ns.type;
            break;
        case CLONE_NEWUSER:
            //            containerPrivate->use_delay_new_user_ns = true;
            break;
        case CLONE_NEWCGROUP:
            containerPrivate->useNewCgroupNs = true;
            break;
        default:
            return -1;
        }
    }

    if (opt.rootLess) {
        flags |= CLONE_NEWUSER;
    }

    startDbusProxy(container.runtime);

    int entryPid = util::platformClone(entryProc, flags, (void *)containerPrivate.get());
    if (entryPid < 0) {
        logErr() << "clone failed" << util::retErrString(entryPid);
        return -1;
    }

    container.reader.reset();

    // FIXME: maybe we need c.opt.child_need_wait?

    ContainerPrivate::dropPermissions();

    // FIXME: parent may dead before this return.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    // FIXME(interactive bash): if need keep interactive shell
    util::waitAllUntil(entryPid);

    return 0;
}

Container::~Container() = default;

} // namespace linglong
