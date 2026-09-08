// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/scheduler.h"

#include "linyaps_box/config/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, scheduler &v)
{
    bool have_policy{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "policy")) {
            auto policy_name = val.get<std::string_view>();
            auto policy_opt = get_enum_table_from<scheduler::policy>().from_name(policy_name);
            if (UNLIKELY(!policy_opt)) {
                throw std::runtime_error(fmt::format("unknown scheduler policy: {}", policy_name));
            }

            v.policy_ = *policy_opt;
            have_policy = true;
        } else if (key_matches(k, "nice")) {
            if (!val.is_null()) {
                val.get_to(v.nice.emplace());
            }
        } else if (key_matches(k, "priority")) {
            if (!val.is_null()) {
                val.get_to(v.priority.emplace());
            }
        } else if (key_matches(k, "flags")) {
            if (!val.is_null()) {
                utils::bitflags<scheduler_flag> flags;
                for (const auto &f : val) {
                    const auto flag_str = f.get<std::string_view>();
                    auto flag_opt = get_enum_table_from<scheduler_flag>().from_name(flag_str);
                    if (UNLIKELY(!flag_opt)) {
                        throw std::runtime_error(
                          fmt::format("unknown scheduler flag: {}", flag_str));
                    }

                    flags |= *flag_opt;
                }

                v.flags = flags;
            }
        } else if (key_matches(k, "runtime")) {
            if (!val.is_null()) {
                val.get_to(v.runtime.emplace());
            }
        } else if (key_matches(k, "deadline")) {
            if (!val.is_null()) {
                val.get_to(v.deadline.emplace());
            }
        } else if (key_matches(k, "period")) {
            if (!val.is_null()) {
                val.get_to(v.period.emplace());
            }
        }
    }

    if (!have_policy) {
        throw std::runtime_error("scheduler.policy is required");
    }
}

void validate(const scheduler &v)
{
    using policy_t = scheduler::policy;

    if (v.nice) {
        if (UNLIKELY(v.policy_ != policy_t::other && v.policy_ != policy_t::batch)) {
            throw std::runtime_error(
              "scheduler.nice can only be specified for SCHED_OTHER or SCHED_BATCH");
        }

        if (UNLIKELY(*v.nice < -20 || *v.nice > 19)) {
            throw std::runtime_error(
              fmt::format("scheduler.nice must be in range [-20, 19]: got {}", *v.nice));
        }
    }

    if (v.priority && *v.priority != 0) {
        if (UNLIKELY(v.policy_ != policy_t::fifo && v.policy_ != policy_t::rr)) {
            throw std::runtime_error(
              "scheduler.priority can only be specified for SCHED_FIFO or SCHED_RR");
        }
    }

    if (v.runtime || v.deadline || v.period) {
        if (UNLIKELY(v.policy_ != policy_t::deadline)) {
            throw std::runtime_error(
              "scheduler runtime/deadline/period can only be specified for SCHED_DEADLINE");
        }
    }
}

} // namespace linyaps_box::config
