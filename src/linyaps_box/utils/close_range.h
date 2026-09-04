// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// NOTE:
// close_range is support since Linux 5.9, glibc 2.34
// but we need to support older kernels and glibc
// so we need to implement it ourselves

#include <sys/syscall.h>

#include <sys/types.h>
#include <unistd.h>

// from linux/close_range.h
#ifndef CLOSE_RANGE_UNSHARE
#  define CLOSE_RANGE_UNSHARE (1U << 1)
#endif

#ifndef CLOSE_RANGE_CLOEXEC
#  define CLOSE_RANGE_CLOEXEC (1U << 2) // after linux 5.11
#endif

namespace linyaps_box::utils {

void close_range(uint first, uint last, int flags);

} // namespace linyaps_box::utils
