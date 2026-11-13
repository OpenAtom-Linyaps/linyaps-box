// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/rlimit.h"

#include <cstdint>
#include <optional>

#include <sys/resource.h>

namespace linyaps_box::specconv {

[[nodiscard]] auto to_rlimit_resource(config::rlimit::type type) noexcept -> int;

// The width of rlim_t is libc/arch specific: musl always uses 64-bit
// with RLIM_INFINITY == UINT64_MAX, glibc is 64-bit on
// 64-bit targets and on 32-bit targets only when _FILE_OFFSET_BITS=64
// (__USE_FILE_OFFSET64), otherwise 32-bit with RLIM_INFINITY == 0xFFFFFFFF.
//
// Only the explicit unlimited forms, UINT64_MAX and the platform's
// RLIM_INFINITY fold onto RLIM_INFINITY.
// Any other value that rlim_t cannot represent yields std::nullopt,
// so an oversized finite limit is never silently promoted to unlimited
// nor truncated.
[[nodiscard]] auto to_rlim_t(std::uint64_t value) noexcept -> std::optional<rlim_t>;

} // namespace linyaps_box::specconv
