/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "platform.h"
#include "logger.h"
#include "util/debug/debug.h"

#include <sched.h>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

namespace linglong {

const int kStackSize = (1024 * 1024);

namespace util {

int PlatformClone(int (*callback)(void *), int flags, void *arg, ...)
{
    char *stack;
    char *stackTop;

    stack = reinterpret_cast<char *>(
        mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
    if (stack == MAP_FAILED) {
        return EXIT_FAILURE;
    }

    stackTop = stack + kStackSize;

    return clone(callback, stackTop, flags, arg);
}

int Exec(const util::str_vec &args, tl::optional<std::vector<std::string>> env_list)
{
    auto targetArgc = args.size();
    const char *targetArgv[targetArgc + 1];
    for (decltype(targetArgc) i = 0; i < targetArgc; i++) {
        targetArgv[i] = args[i].c_str();
    }
    targetArgv[targetArgc] = nullptr;

    auto targetEnvc = env_list.has_value() ? env_list->size() : 0;
    const char **targetEnvv = targetEnvc ? new const char *[targetEnvc + 1] : nullptr;
    if (targetEnvv) {
        for (decltype(targetEnvc) i = 0; i < targetEnvc; i++) {
            targetEnvv[i] = env_list.value().at(i).c_str();
        }
        targetEnvv[targetEnvc] = nullptr;
    }

    logDbg() << "execve" << targetArgv[0] << " in pid:" << getpid();

    int ret = execve(targetArgv[0], const_cast<char **>(targetArgv), const_cast<char **>(targetEnvv));

    delete[] targetEnvv;

    return ret;
}

} // namespace util
} // namespace linglong