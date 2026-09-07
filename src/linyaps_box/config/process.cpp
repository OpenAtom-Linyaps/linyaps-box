// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/process.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep
#include "linyaps_box/utils/environ.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <bitset>
#include <stdexcept>

#include <sys/types.h>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, process &v)
{
    bool have_cwd{ false };
    bool have_args{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "terminal")) {
            if (!val.is_null()) {
                val.get_to(v.terminal.emplace());
            }
        } else if (key_matches(k, "consoleSize")) {
            if (!val.is_null()) {
                val.get_to(v.console_size_.emplace());
            }
        } else if (key_matches(k, "cwd")) {
            val.get_to(v.cwd);
            have_cwd = true;
        } else if (key_matches(k, "env")) {
            if (!val.is_null()) {
                val.get_to(v.env.emplace());
            }
        } else if (key_matches(k, "args")) {
            val.get_to(v.args);
            have_args = true;
        } else if (key_matches(k, "rlimits")) {
            if (!val.is_null()) {
                val.get_to(v.rlimits.emplace());
            }
        } else if (key_matches(k, "apparmorProfile")) {
            if (!val.is_null()) {
                val.get_to(v.apparmor_profile.emplace());
            }
        } else if (key_matches(k, "capabilities")) {
            if (!val.is_null()) {
                val.get_to(v.capabilities_.emplace());
            }
        } else if (key_matches(k, "noNewPrivileges")) {
            if (!val.is_null()) {
                val.get_to(v.no_new_privileges.emplace());
            }
        } else if (key_matches(k, "oomScoreAdj")) {
            if (!val.is_null()) {
                val.get_to(v.oom_score_adj.emplace());
            }
        } else if (key_matches(k, "scheduler")) {
            if (!val.is_null()) {
                val.get_to(v.scheduler_.emplace());
            }
        } else if (key_matches(k, "selinuxLabel")) {
            if (!val.is_null()) {
                val.get_to(v.selinux_label.emplace());
            }
        } else if (key_matches(k, "ioPriority")) {
            if (!val.is_null()) {
                val.get_to(v.io_priority_.emplace());
            }
        } else if (key_matches(k, "execCPUAffinity")) {
            if (!val.is_null()) {
                val.get_to(v.exec_cpu_affinity_.emplace());
            }
        } else if (key_matches(k, "user")) {
            if (!val.is_null()) {
                val.get_to(v.user_);
            } else {
                v.user_ = { };
            }
        }
    }

    // consoleSize only meaningful with terminal enabled
    if (!v.terminal.value_or(false)) {
        v.console_size_.reset();
    }

    if (UNLIKELY(!have_cwd)) {
        throw std::runtime_error("process.cwd is required");
    }

    if (UNLIKELY(!have_args)) {
        throw std::runtime_error("process.args is required");
    }
}

void validate(const process &v)
{
    // Capability names are deliberately not validated here. Unlike the
    // closed value sets (seccomp actions, namespace types, ...), the OCI
    // runtime spec (config.md, "capabilities") mandates that a value which
    // cannot be mapped to a relevant kernel interface MUST be logged as a
    // warning and the runtime SHOULD NOT fail the container because of it.
    // security/privilege.cpp::parse_names implements exactly that behavior,
    // so unknown names are surfaced at runtime as warnings, not here.
    if (v.capabilities_) {
#ifndef LINYAPS_BOX_ENABLE_CAP
        throw std::runtime_error("capabilities support is not compiled in");
#endif
    }

    validate(v.user_);

    if (UNLIKELY(!v.cwd.is_absolute())) {
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
            auto type_val = static_cast<size_t>(rl.type_);
            if (UNLIKELY(type_val >= seen.size())) {
                throw std::runtime_error("invalid rlimit type value out of range");
            }

            if (UNLIKELY(seen.test(type_val))) {
                throw std::runtime_error(fmt::format("duplicate rlimit type: {}", rl.type_));
            }

            seen.set(type_val);
        }
    }

    if (v.scheduler_) {
        validate(*v.scheduler_);
    }

    if (v.io_priority_) {
        validate(*v.io_priority_);
    }
}

auto process::parse(std::string_view content) -> process
{
    auto process_config = nlohmann::json::parse(content).get<process>();
    validate(process_config);
    return process_config;
}

auto process::parse(const std::filesystem::path &path) -> process
{
    utils::uninit_vector<std::byte> buf;
    auto len = read_json_to(path, buf);

    const std::string_view content_view{ reinterpret_cast<const char *>(buf.data()), // NOLINT
                                         len };

    return parse(content_view);
}

} // namespace linyaps_box::config
