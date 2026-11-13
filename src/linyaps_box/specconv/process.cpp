// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/specconv/process.h"

#include "linyaps_box/utils/utils.h"

#include <cassert>
#include <limits>

#include <sys/resource.h>

namespace linyaps_box::specconv {

using namespace linyaps_box::config;

auto to_rlimit_resource(rlimit::type type) noexcept -> int
{
    using type_t = rlimit::type;
    switch (type) {
    case type_t::as:
        return RLIMIT_AS;
    case type_t::core:
        return RLIMIT_CORE;
    case type_t::cpu:
        return RLIMIT_CPU;
    case type_t::data:
        return RLIMIT_DATA;
    case type_t::fsize:
        return RLIMIT_FSIZE;
    case type_t::locks:
        return RLIMIT_LOCKS;
    case type_t::memlock:
        return RLIMIT_MEMLOCK;
    case type_t::msgqueue:
        return RLIMIT_MSGQUEUE;
    case type_t::nice:
        return RLIMIT_NICE;
    case type_t::nofile:
        return RLIMIT_NOFILE;
    case type_t::nproc:
        return RLIMIT_NPROC;
    case type_t::rss:
        return RLIMIT_RSS;
    case type_t::rtprio:
        return RLIMIT_RTPRIO;
    case type_t::rttime:
        return RLIMIT_RTTIME;
    case type_t::sigpending:
        return RLIMIT_SIGPENDING;
    case type_t::stack:
        return RLIMIT_STACK;
    }

    utils::unreachable("unknown rlimit type");
}

auto to_rlim_t(std::uint64_t value) noexcept -> std::optional<rlim_t>
{
    constexpr auto native_infinity = static_cast<std::uint64_t>(RLIM_INFINITY);
    constexpr auto uint64_max = std::numeric_limits<std::uint64_t>::max();

    if (value == native_infinity || value == uint64_max) {
        return RLIM_INFINITY;
    }

    if (value > native_infinity) {
        return std::nullopt;
    }

    return static_cast<rlim_t>(value);
}

} // namespace linyaps_box::specconv
