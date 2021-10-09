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

#include <sched.h>
#include <cstdlib>
#include <sys/mman.h>

namespace linglong {

const int kStackSize = (1024 * 1024);

int platformClone(int (*callback)(void *), int flags, void *arg, ...)
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

} // namespace linglong