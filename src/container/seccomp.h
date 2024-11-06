/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#if LINYAPS_BOX_ENABLE_SECCOMP

#include "util/oci_runtime.h"

namespace linglong {
int ConfigSeccomp(const std::optional<linglong::Seccomp> &seccomp);
}

#endif
