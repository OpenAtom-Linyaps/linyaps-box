// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/ns_flags.h"

#include "linyaps_box/utils/utils.h"

#include <csignal>

#include <sched.h>

namespace linyaps_box::utils {

using namespace linyaps_box::config;

auto to_clone_flag(ns::type type) noexcept -> unsigned int
{
    using type_t = ns::type;
    switch (type) {
    case type_t::ipc:
        return CLONE_NEWIPC;
    case type_t::uts:
        return CLONE_NEWUTS;
    case type_t::mount:
        return CLONE_NEWNS;
    case type_t::pid:
        return CLONE_NEWPID;
    case type_t::net:
        return CLONE_NEWNET;
    case type_t::user:
        return CLONE_NEWUSER;
    case type_t::cgroup:
        return CLONE_NEWCGROUP;
    case type_t::time:
#ifdef CLONE_NEWTIME
        return CLONE_NEWTIME;
#else
        return 0x00000080;
#endif
    }

    __builtin_unreachable();
}

auto generate_clone_flags(const std::optional<std::vector<ns>> &namespaces) noexcept -> unsigned int
{
    unsigned flag = SIGCHLD;
    if (!namespaces) {
        return flag;
    }

    for (const auto &ns : *namespaces) {
        if (ns.path) {
            // A namespace entry with a `path` is joined via setns(2), not
            // created, so it contributes no clone flag.
            continue;
        }

        flag |= to_clone_flag(ns.type_);
    }

    return flag;
}

} // namespace linyaps_box::utils
