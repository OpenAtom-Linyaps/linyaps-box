/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include <grp.h>
#include <sched.h>
#include <unistd.h>
#include <pwd.h>

#include "util/logger.h"
#include "util/filesystem.h"
#include "util/semaphore.h"

#define STACK_SIZE (1024 * 1024)

using namespace linglong;

struct Context {
    Context(int argc, char **argv)
    {
        const std::string execute = argv[0];

        std::string targetRoot = "/run/host/fake/sandbox/";
        if (getenv("LL_FAKE_SANDBOX_ROOT")) {
            targetRoot = getenv("LL_FAKE_SANDBOX_ROOT");
        }
        // todo: encoding path;
        auto targetPath = util::fs::path(targetRoot) / execute;
        targetExecute = targetPath.string();

        for (int i = 0; i < argc; ++i) {
            args.push_back(argv[i]);
        }
    }

    std::string targetExecute;
    util::strVec args;
    int semID {};
};

int entryProc(void *arg)
{
    auto &c = *reinterpret_cast<Context *>(arg);

    auto ret = -1;

    auto unshareFlags = CLONE_NEWUSER | CLONE_NEWNS;
    ret = unshare(unshareFlags);
    if (0 != ret) {
        logErr() << "unshare failed" << unshareFlags << util::retErrString(ret);
    }

    Semaphore s(c.semID);
    logDbg() << "wait parent start";
    s.vrijgeven();
    s.passeren();
    logDbg() << "parent process finish";

    prctl(PR_SET_PDEATHSIG, SIGKILL);

    int execPid = fork();
    if (execPid < 0) {
        logErr() << "fork failed" << util::retErrString(execPid);
        return -1;
    }

    if (execPid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        logDbg() << "c.r.process.args:" << c.args;

        //        FIXME: groups=0(root),65534(nogroup)
        //        __gid_t newgid[1] = {getgid()};
        //        setgroups(1, newgid);

        size_t targetArgc = c.args.size();
        char *targetArgv[targetArgc + 1];
        for (size_t i = 0; i < targetArgc; i++) {
            targetArgv[i] = (char *)(c.args[i].c_str());
        }
        targetArgv[targetArgc] = nullptr;

        logDbg() << "execve" << c.targetExecute << targetArgv[0] << " with pid:" << getpid();

        ret = execv(c.targetExecute.c_str(), const_cast<char **>(targetArgv));
        if (0 != ret) {
            logErr() << "execv failed" << util::retErrString(ret);
        }
        logDbg() << "execv return" << util::retErrString(ret);

        return ret;
    }

    // FIXME(interactive bash): if need keep interactive shell
    while (true) {
        int childPid = waitpid(-1, nullptr, 0);
        if (childPid > 0) {
            logDbg() << "wait" << childPid << "exit";
        }
        if (childPid < 0) {
            logInf() << "all child exit" << childPid;
            return 0;
        }
    }
}

int main(int argc, char **argv)
{
    Context ctx(argc, argv);

    char *stack;
    char *stackTop;

    stack = reinterpret_cast<char *>(
        mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
    if (stack == MAP_FAILED) {
        return -1;
    }

    stackTop = stack + STACK_SIZE;

    int flags = SIGCHLD;

    ctx.semID = getpid();
    Semaphore s(ctx.semID);
    s.init();

    int entryPid = clone(entryProc, stackTop, flags, (void *)&ctx);
    if (entryPid == -1) {
        logErr() << "clone failed" << util::retErrString(entryPid);
        return -1;
    }

    logDbg() << "wait child start" << entryPid;
    s.passeren();
    // write gid map
    auto setgroupsPath = util::format("/proc/%d/setgroups", entryPid);
    std::ofstream setgroupsFile(setgroupsPath);
    setgroupsFile << "deny";
    setgroupsFile.close();

    std::ofstream gidMapFile(util::format("/proc/%d/gid_map", entryPid));
    gidMapFile << util::format("%lu %lu %lu\n", 0, getgid(), 1);
    gidMapFile.close();

    setgroupsFile.open(setgroupsPath);
    setgroupsFile << "allow";
    setgroupsFile.close();

    std::ofstream uidMapFile(util::format("/proc/%d/uid_map", entryPid));
    uidMapFile << util::format("%lu %lu %lu\n", 0, getuid(), 1);
    uidMapFile.close();

    logDbg() << "notify child continue" << entryPid;
    s.vrijgeven();

    // FIXME(interactive bash): if need keep interactive shell
    if (waitpid(-1, nullptr, 0) < 0) {
        logErr() << "waitpid failed" << util::errnoString();
        return -1;
    }
    logInf() << "wait" << entryPid << "finish";

    return 0;
}