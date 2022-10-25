/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_PLATFORM_H_
#define LINGLONG_BOX_SRC_UTIL_PLATFORM_H_

#include "3party/optional/optional.hpp"

#include "common.h"

namespace linglong {

namespace util {

int platformClone(int (*callback)(void *), int flags, void *arg, ...);

int exec(const util::strVec &args, tl::optional<std::vector<std::string>> envList);

void wait(const int pid);
void waitAll();
void waitAllUntil(const int pid);

} // namespace util

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_PLATFORM_H_ */
