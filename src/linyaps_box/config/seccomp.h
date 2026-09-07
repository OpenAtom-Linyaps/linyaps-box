// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct seccomp
{
    enum class action : std::uint8_t {
        allow,
        errno_,
        kill,
        kill_process,
        kill_thread,
        log,
        notify,
        trace,
        trap
    };

    enum class arch : uint8_t {
        x86,
        x86_64,
        x32,
        arm,
        aarch64,
        mips,
        mips64,
        mips64n32,
        mipsel,
        mipsel64,
        mipsel64n32,
        ppc,
        ppc64,
        ppc64le,
        s390,
        s390x,
        parisc,
        parisc64,
        riscv64,
        loongarch64,
        m68k,
        sh,
        sheb,
    };

    enum class flag : std::uint8_t {
        none = 0U,
        tsync = (1U << 0),
        log = (1U << 1),
        spec_allow = (1U << 2),
        wait_killable_recv = (1U << 3),
    };

    struct syscall
    {
        std::vector<std::string> names;
        action action_;
        std::optional<std::uint32_t> errno_ret;

        struct arg
        {
            enum class op : uint8_t { eq, ne, lt, le, gt, ge, masked_eq };

            std::uint32_t index{ 0 };
            uint64_t value{ 0 };
            std::optional<uint64_t> value_two;
            op op_;
        };

        std::optional<std::vector<arg>> args;
    };

    std::optional<std::vector<arch>> architectures;
    std::optional<std::string> listener_metadata;
    std::optional<std::filesystem::path> listener_path;
    std::optional<std::vector<syscall>> syscalls;
    std::optional<std::uint32_t> default_errno_ret;
    std::optional<flag> flags;
    action default_action;
};

LINYAPS_REGISTER_ENUM_TABLE(seccomp::action,
                            9,
                            { seccomp::action::allow, "SCMP_ACT_ALLOW" },
                            { seccomp::action::errno_, "SCMP_ACT_ERRNO" },
                            { seccomp::action::kill, "SCMP_ACT_KILL" },
                            { seccomp::action::kill_process, "SCMP_ACT_KILL_PROCESS" },
                            { seccomp::action::kill_thread, "SCMP_ACT_KILL_THREAD" },
                            { seccomp::action::log, "SCMP_ACT_LOG" },
                            { seccomp::action::notify, "SCMP_ACT_NOTIFY" },
                            { seccomp::action::trace, "SCMP_ACT_TRACE" },
                            { seccomp::action::trap, "SCMP_ACT_TRAP" })

LINYAPS_REGISTER_ENUM_TABLE(seccomp::arch,
                            23,
                            { seccomp::arch::x86, "SCMP_ARCH_X86" },
                            { seccomp::arch::x86_64, "SCMP_ARCH_X86_64" },
                            { seccomp::arch::x32, "SCMP_ARCH_X32" },
                            { seccomp::arch::arm, "SCMP_ARCH_ARM" },
                            { seccomp::arch::aarch64, "SCMP_ARCH_AARCH64" },
                            { seccomp::arch::mips, "SCMP_ARCH_MIPS" },
                            { seccomp::arch::mips64, "SCMP_ARCH_MIPS64" },
                            { seccomp::arch::mips64n32, "SCMP_ARCH_MIPS64N32" },
                            { seccomp::arch::mipsel, "SCMP_ARCH_MIPSEL" },
                            { seccomp::arch::mipsel64, "SCMP_ARCH_MIPSEL64" },
                            { seccomp::arch::mipsel64n32, "SCMP_ARCH_MIPSEL64N32" },
                            { seccomp::arch::ppc, "SCMP_ARCH_PPC" },
                            { seccomp::arch::ppc64, "SCMP_ARCH_PPC64" },
                            { seccomp::arch::ppc64le, "SCMP_ARCH_PPC64LE" },
                            { seccomp::arch::s390, "SCMP_ARCH_S390" },
                            { seccomp::arch::s390x, "SCMP_ARCH_S390X" },
                            { seccomp::arch::parisc, "SCMP_ARCH_PARISC" },
                            { seccomp::arch::parisc64, "SCMP_ARCH_PARISC64" },
                            { seccomp::arch::riscv64, "SCMP_ARCH_RISCV64" },
                            { seccomp::arch::loongarch64, "SCMP_ARCH_LOONGARCH64" },
                            { seccomp::arch::m68k, "SCMP_ARCH_M68K" },
                            { seccomp::arch::sh, "SCMP_ARCH_SH" },
                            { seccomp::arch::sheb, "SCMP_ARCH_SHEB" })

LINYAPS_ENABLE_BITMASK_ENUM(seccomp::flag);

LINYAPS_REGISTER_ENUM_TABLE(seccomp::flag,
                            4,
                            { seccomp::flag::tsync, "SECCOMP_FILTER_FLAG_TSYNC" },
                            { seccomp::flag::log, "SECCOMP_FILTER_FLAG_LOG" },
                            { seccomp::flag::spec_allow, "SECCOMP_FILTER_FLAG_SPEC_ALLOW" },
                            { seccomp::flag::wait_killable_recv,
                              "SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV" })

LINYAPS_REGISTER_ENUM_TABLE(seccomp::syscall::arg::op,
                            7,
                            { seccomp::syscall::arg::op::eq, "SCMP_CMP_EQ" },
                            { seccomp::syscall::arg::op::ne, "SCMP_CMP_NE" },
                            { seccomp::syscall::arg::op::lt, "SCMP_CMP_LT" },
                            { seccomp::syscall::arg::op::le, "SCMP_CMP_LE" },
                            { seccomp::syscall::arg::op::gt, "SCMP_CMP_GT" },
                            { seccomp::syscall::arg::op::ge, "SCMP_CMP_GE" },
                            { seccomp::syscall::arg::op::masked_eq, "SCMP_CMP_MASKED_EQ" })

void from_json(const nlohmann::json &j, seccomp::syscall::arg &v);

void from_json(const nlohmann::json &j, seccomp::syscall &v);

void from_json(const nlohmann::json &j, seccomp &v);

void validate(const seccomp::syscall &v);

void validate(const seccomp &v);

} // namespace linyaps_box::config
