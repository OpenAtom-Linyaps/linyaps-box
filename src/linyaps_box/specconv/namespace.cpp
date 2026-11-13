// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/specconv/namespace.h"

#include "linyaps_box/utils/utils.h"

#include <csignal> // IWYU pragma: keep

#include <sched.h>

namespace linyaps_box::specconv {

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

    utils::unreachable("unknown ns type");
}

auto generate_clone_flags(const std::optional<std::vector<ns>> &namespaces) noexcept -> unsigned int
{
    unsigned flag{ SIGCHLD };
    if (!namespaces) {
        return flag;
    }

    for (const auto &ns : *namespaces) {
        if (ns.path) {
            continue;
        }

        flag |= to_clone_flag(ns.type_);
    }

    return flag;
}

auto to_proc_ns_name(ns::type type) noexcept -> std::string_view
{
    switch (type) {
    case ns::type::ipc:
        return "ipc";
    case ns::type::uts:
        return "uts";
    case ns::type::mount:
        return "mnt";
    case ns::type::pid:
        return "pid";
    case ns::type::net:
        return "net";
    case ns::type::user:
        return "user";
    case ns::type::cgroup:
        return "cgroup";
    case ns::type::time:
        return "time";
    }

    utils::unreachable("unknown ns name");
}

} // namespace linyaps_box::specconv
