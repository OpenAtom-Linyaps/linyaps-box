/*
 * Copyright (c) 2021-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_SECCOMP_H_
#define LINGLONG_BOX_SRC_CONTAINER_SECCOMP_H_

#include "util/oci_runtime.h"

namespace linglong {
int configSeccomp(const tl::optional<linglong::Seccomp> &seccomp);
}

#endif /* LINGLONG_BOX_SRC_CONTAINER_SECCOMP_H_ */
