// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/seccomp.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/format.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(seccomp::syscall::arg,
                          seccomp_syscall_arg_schema,
                          "seccomp.syscall.arg",
                          field<&seccomp::syscall::arg::index>("index").required(),
                          field<&seccomp::syscall::arg::value>("value").required(),
                          field<&seccomp::syscall::arg::value_two>("valueTwo").ignore_null(),
                          field<&seccomp::syscall::arg::op_>("op").required())

LINYAPS_BOX_CONFIG_SCHEMA(seccomp::syscall,
                          seccomp_syscall_schema,
                          "seccomp.syscall",
                          field<&seccomp::syscall::names>("names").required(),
                          field<&seccomp::syscall::action_>("action").required(),
                          field<&seccomp::syscall::errno_ret>("errnoRet").ignore_null(),
                          field<&seccomp::syscall::args>("args").ignore_null())

LINYAPS_BOX_CONFIG_SCHEMA(seccomp,
                          seccomp_schema,
                          "seccomp",
                          field<&seccomp::default_action>("defaultAction").required(),
                          field<&seccomp::default_errno_ret>("defaultErrnoRet").ignore_null(),
                          field<&seccomp::architectures>("architectures").ignore_null(),
                          field<&seccomp::flags>("flags").ignore_null(),
                          field<&seccomp::listener_path>("listenerPath").ignore_null(),
                          field<&seccomp::listener_metadata>("listenerMetadata").ignore_null(),
                          field<&seccomp::syscalls>("syscalls").ignore_null())

void validate(const seccomp::syscall &v)
{
    require(!v.names.empty(), "seccomp syscall names must not be empty");
    require(!v.errno_ret || v.action_ == seccomp::action::errno_
              || v.action_ == seccomp::action::trace,
            "seccomp syscall errnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
}

void validate(const seccomp &v)
{
    require(!v.default_errno_ret || v.default_action == seccomp::action::errno_
              || v.default_action == seccomp::action::trace,
            "seccomp defaultErrnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    require(v.default_action != seccomp::action::notify || v.listener_path,
            "seccomp SCMP_ACT_NOTIFY requires listenerPath");
    require(!v.listener_metadata || v.listener_path,
            "seccomp listenerMetadata requires listenerPath to be set");
    if (v.syscalls) {
        std::for_each(v.syscalls->cbegin(), v.syscalls->cend(), [](const auto &syscall) {
            validate(syscall);
        });
    }
}

} // namespace linyaps_box::config
