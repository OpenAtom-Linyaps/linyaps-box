// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/validate.h"

#include "linyaps_box/config.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <algorithm>
#include <bitset>
#include <stdexcept>
#include <utility>

namespace linyaps_box {

namespace {

auto validate(const oci_config::process_t::io_priority_t &v) -> void
{
    if (UNLIKELY(v.priority < 0 || v.priority > 7)) {
        throw std::runtime_error(
          fmt::format("io priority must be in range [0, 7], got: {}", v.priority));
    }
}

auto validate(const oci_config::process_t::scheduler_t &v) -> void
{
    using policy_t = oci_config::process_t::scheduler_t::policy_t;

    if (v.nice) {
        if (UNLIKELY(*v.nice < -20 || *v.nice > 19)) {
            throw std::runtime_error(
              fmt::format("scheduler.nice must be in range [-20, 19]: got {}", *v.nice));
        }
    }

    if (v.priority && *v.priority != 0) {
        if (UNLIKELY(v.policy != policy_t::FIFO && v.policy != policy_t::RR)) {
            throw std::runtime_error(
              "scheduler.priority can only be specified for SCHED_FIFO or SCHED_RR");
        }
    }

    if (v.runtime || v.deadline || v.period) {
        if (UNLIKELY(v.policy != policy_t::DEADLINE)) {
            throw std::runtime_error(
              "scheduler runtime/deadline/period can only be specified for SCHED_DEADLINE");
        }
    }
}

void validate(const oci_config::process_t &v)
{
    if (v.capabilities) {
#ifndef LINYAPS_BOX_ENABLE_CAP
        throw std::runtime_error("capabilities support is not compiled in");
#endif
    }

    if (!v.cwd.is_absolute()) {
        throw std::runtime_error(
          fmt::format("process.cwd must be an absolute path, got: {}", v.cwd));
    }

    if (v.env) {
        auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), utils::is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error(
              fmt::format("process.env contains a invalid env: {}", *invalid));
        }
    }

    if (UNLIKELY(v.args.empty())) {
        throw std::runtime_error("process.args must not be empty");
    }

    if (v.rlimits) {
        std::bitset<16> seen;
        for (const auto &rl : *v.rlimits) {
            auto type_val = static_cast<size_t>(rl.type);
            if (UNLIKELY(type_val >= seen.size())) {
                throw std::runtime_error("invalid rlimit type value out of range");
            }

            if (UNLIKELY(seen.test(type_val))) {
                throw std::runtime_error(
                  fmt::format("duplicate rlimit type: {}", to_string_view(rl.type)));
            }

            seen.set(type_val);
        }
    }

    if (v.scheduler) {
        validate(*v.scheduler);
    }

    if (v.io_priority) {
        validate(*v.io_priority);
    }
}

void validate(const oci_config::linux_t::seccomp_t::syscall_t &v)
{
    if (v.names.empty()) {
        throw std::runtime_error("seccomp syscall names must not be empty");
    }

    using action_t = oci_config::linux_t::seccomp_t::action_t;
    if (v.errno_ret && v.action != action_t::ERRNO && v.action != action_t::TRACE) {
        throw std::runtime_error(
          "seccomp syscall errnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    }
}

void validate(const oci_config::linux_t::seccomp_t &v)
{
    using action_t = oci_config::linux_t::seccomp_t::action_t;

    if (UNLIKELY(v.default_errno_ret && v.default_action != action_t::ERRNO
                 && v.default_action != action_t::TRACE)) {
        throw std::runtime_error(
          "seccomp defaultErrnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    }

    if (UNLIKELY(v.default_action == action_t::NOTIFY && !v.listener_path)) {
        throw std::runtime_error("seccomp SCMP_ACT_NOTIFY requires listenerPath");
    }

    if (UNLIKELY(v.listener_metadata && !v.listener_path)) {
        throw std::runtime_error("seccomp listenerMetadata requires listenerPath to be set");
    }

    if (v.syscalls) {
        for (const auto &s : *v.syscalls) {
            validate(s);
        }
    }
}

void validate(const std::vector<oci_config::linux_t::namespace_t> &namespaces)
{
    unsigned int seen{ 0 };
    for (const auto &ns : namespaces) {
        auto val = static_cast<unsigned int>(ns.type_);
        if (UNLIKELY((seen & val) != 0)) {
            throw std::runtime_error(
              fmt::format("duplicate namespace type: {}", to_string_view(ns.type_)));
        }

        seen |= val;
    }
}

void validate(const oci_config::linux_t &v)
{
    if (v.namespaces) {
        validate(*v.namespaces);
    }

    if (v.seccomp) {
#ifdef LINYAPS_BOX_ENABLE_SECCOMP
        validate(*v.seccomp);
#else
        throw std::runtime_error("seccomp support is not compiled in");
#endif
    }

    if (v.masked_paths) {
        for (const auto &p : *v.masked_paths) {
            if (UNLIKELY(!p.is_absolute())) {
                throw std::runtime_error(
                  fmt::format("maskedPaths must be absolute paths, got: {}", p));
            }
        }
    }

    if (v.readonly_paths) {
        for (const auto &p : *v.readonly_paths) {
            if (UNLIKELY(!p.is_absolute())) {
                throw std::runtime_error(
                  fmt::format("readonlyPaths must be absolute paths, got: {}", p));
            }
        }
    }

    if (v.resources) {
        if (v.resources->memory) {
            if (UNLIKELY(v.resources->memory->swappiness
                         && *v.resources->memory->swappiness > 100)) {
                throw std::runtime_error(
                  fmt::format("memory.swappiness must be in range [0, 100], got: {}",
                              *v.resources->memory->swappiness));
            }
        }

        if (v.resources->cpu) {
            const auto &cpu = *v.resources->cpu;
            if (UNLIKELY(cpu.quota && *cpu.quota > 0 && cpu.burst
                         && *cpu.burst > static_cast<uint64_t>(*cpu.quota))) {
                throw std::runtime_error("cpu.quota must be no smaller than cpu.burst");
            }
        }
    }
}

void validate(const oci_config::mount_t &v)
{
    if (UNLIKELY(v.uid_mappings.has_value() != v.gid_mappings.has_value())) {
        throw std::runtime_error(
          "uidMappings and gidMappings on mounts must be specified together");
    }
}

void validate(const oci_config::hooks_t::hook_t &v)
{
    if (UNLIKELY(!v.path.is_absolute())) {
        throw std::runtime_error("hook path must be absolute");
    }

    if (v.env) {
        auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), utils::is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error(fmt::format("hook.env contains a invalid env: {}", *invalid));
        }
    }

    if (v.timeout && *v.timeout <= 0) {
        throw std::runtime_error("hook timeout must be greater than zero");
    }
}

void validate(const oci_config::hooks_t &v)
{
    auto validate_hooks = [](const auto &hooks) {
        for (const auto &h : hooks) {
            validate(h);
        }
    };

    if (v.prestart) {
        validate_hooks(*v.prestart);
    }

    if (v.create_runtime) {
        validate_hooks(*v.create_runtime);
    }

    if (v.create_container) {
        validate_hooks(*v.create_container);
    }

    if (v.start_container) {
        validate_hooks(*v.start_container);
    }

    if (v.poststart) {
        validate_hooks(*v.poststart);
    }

    if (v.poststop) {
        validate_hooks(*v.poststop);
    }
}

} // namespace

void validate(const oci_config &config)
{
    if (config.process) {
        validate(*config.process);
    }

    if (config.linux) {
        validate(*config.linux);
    }

    if (config.hooks) {
        validate(*config.hooks);
    }

    for (const auto &m : config.mounts) {
        validate(m);
    }

    if (config.annotations) {
        for (const auto &[key, value] : *config.annotations) {
            (void)value;
            if (key.empty()) {
                throw std::runtime_error("annotations keys must not be empty");
            }
        }
    }
}

} // namespace linyaps_box
