/*
 * Copyright (c) 2021-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "util/oci-runtime.h"

namespace linglong {
int configSeccomp(const tl::optional<linglong::Seccomp> &seccomp);
}