// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/seccomp.h"

#include "linyaps_box/config/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace linyaps_box::config {
constexpr auto seccomp_action_table = get_enum_table_from<seccomp::action>();

void from_json(const nlohmann::json &j, seccomp::syscall::arg &v)
{
    j.at("index").get_to(v.index);
    j.at("value").get_to(v.value);

    if (auto value_two_it = j.find("valueTwo");
        value_two_it != j.end() && !value_two_it->is_null()) {
        value_two_it->get_to(v.value_two.emplace());
    }

    auto op_name = j.at("op").get<std::string_view>();
    auto op_opt = get_enum_table_from<seccomp::syscall::arg::op>().from_name(op_name);
    if (UNLIKELY(!op_opt)) {
        throw std::runtime_error(fmt::format("unknown seccomp arg op: {}", op_name));
    }

    v.op_ = *op_opt;
}

void from_json(const nlohmann::json &j, seccomp::syscall &v)
{
    j.at("names").get_to(v.names);

    auto action_name = j.at("action").get<std::string_view>();
    auto action_opt = seccomp_action_table.from_name(action_name);
    if (UNLIKELY(!action_opt)) {
        throw std::runtime_error(fmt::format("unknown seccomp action: {}", action_name));
    }
    v.action_ = *action_opt;

    if (auto it = j.find("errnoRet"); it != j.end() && !it->is_null()) {
        it->get_to(v.errno_ret.emplace());
    }

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }
}

void from_json(const nlohmann::json &j, seccomp &v)
{
    bool have_default_action{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "defaultAction")) {
            auto action_name = val.get<std::string_view>();
            auto action_opt = seccomp_action_table.from_name(action_name);
            if (UNLIKELY(!action_opt)) {
                throw std::runtime_error(fmt::format("unknown seccomp action: {}", action_name));
            }

            v.default_action = *action_opt;
            have_default_action = true;
        } else if (key_matches(k, "defaultErrnoRet")) {
            if (!val.is_null()) {
                val.get_to(v.default_errno_ret.emplace());
            }
        } else if (key_matches(k, "architectures")) {
            if (!val.is_null()) {
                std::vector<seccomp::arch> archs;
                archs.reserve(val.size());

                for (const auto &elem : val) {
                    auto arch_str = elem.get<std::string_view>();
                    auto arch_opt = get_enum_table_from<seccomp::arch>().from_name(arch_str);

                    if (UNLIKELY(!arch_opt)) {
                        throw std::runtime_error(
                          fmt::format("unknown seccomp architecture: {}", arch_str));
                    }

                    archs.push_back(*arch_opt);
                }

                v.architectures = std::move(archs);
            }
        } else if (key_matches(k, "flags")) {
            if (!val.is_null()) {
                utils::bitflags<seccomp_flag> flags;
                for (const auto &f : val) {
                    const auto flag_str = f.get<std::string_view>();
                    auto flag_opt = get_enum_table_from<seccomp_flag>().from_name(flag_str);
                    if (UNLIKELY(!flag_opt)) {
                        throw std::runtime_error(fmt::format("unknown seccomp flag: {}", flag_str));
                    }

                    flags |= *flag_opt;
                }

                v.flags = flags;
            }
        } else if (key_matches(k, "listenerPath")) {
            if (!val.is_null()) {
                val.get_to(v.listener_path.emplace());
            }
        } else if (key_matches(k, "listenerMetadata")) {
            if (!val.is_null()) {
                val.get_to(v.listener_metadata.emplace());
            }
        } else if (key_matches(k, "syscalls")) {
            if (!val.is_null()) {
                val.get_to(v.syscalls.emplace());
            }
        }
    }

    if (UNLIKELY(!have_default_action)) {
        throw std::runtime_error("seccomp.defaultAction is required");
    }
}

void validate(const seccomp::syscall &v)
{
    if (UNLIKELY(v.names.empty())) {
        throw std::runtime_error("seccomp syscall names must not be empty");
    }

    if (UNLIKELY(v.errno_ret && v.action_ != seccomp::action::errno_
                 && v.action_ != seccomp::action::trace)) {
        throw std::runtime_error(
          "seccomp syscall errnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    }
}

void validate(const seccomp &v)
{
    if (UNLIKELY(v.default_errno_ret && v.default_action != seccomp::action::errno_
                 && v.default_action != seccomp::action::trace)) {
        throw std::runtime_error(
          "seccomp defaultErrnoRet is only valid with SCMP_ACT_ERRNO or SCMP_ACT_TRACE");
    }

    if (UNLIKELY(v.default_action == seccomp::action::notify && !v.listener_path)) {
        throw std::runtime_error("seccomp SCMP_ACT_NOTIFY requires listenerPath");
    }

    if (UNLIKELY(v.listener_metadata && !v.listener_path)) {
        throw std::runtime_error("seccomp listenerMetadata requires listenerPath to be set");
    }

    if (!v.syscalls) {
        return;
    }

    std::for_each(v.syscalls->cbegin(), v.syscalls->cend(), [](const auto &syscall) {
        validate(syscall);
    });
}

} // namespace linyaps_box::config
