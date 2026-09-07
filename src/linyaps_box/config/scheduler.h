// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct scheduler
{
    enum class policy : uint8_t { other, fifo, rr, batch, iso, idle, deadline };

    enum class flag : std::uint8_t {
        none = 0U,
        reset_on_fork = (1U << 0),
        reclaim = (1U << 1),
        dl_overrun = (1U << 2),
        keep_policy = (1U << 3),
        keep_params = (1U << 4),
        util_clamp_min = (1U << 5),
        util_clamp_max = (1U << 6),
    };

    std::optional<uint64_t> runtime;
    std::optional<uint64_t> deadline;
    std::optional<uint64_t> period;
    std::optional<int32_t> nice;
    std::optional<int32_t> priority;
    std::optional<flag> flags;
    policy policy_;
};

LINYAPS_ENABLE_BITMASK_ENUM(scheduler::flag);

LINYAPS_REGISTER_ENUM_TABLE(scheduler::policy,
                            7,
                            { scheduler::policy::other, "SCHED_OTHER" },
                            { scheduler::policy::fifo, "SCHED_FIFO" },
                            { scheduler::policy::rr, "SCHED_RR" },
                            { scheduler::policy::batch, "SCHED_BATCH" },
                            { scheduler::policy::iso, "SCHED_ISO" },
                            { scheduler::policy::idle, "SCHED_IDLE" },
                            { scheduler::policy::deadline, "SCHED_DEADLINE" })

LINYAPS_REGISTER_ENUM_TABLE(scheduler::flag,
                            7,
                            { scheduler::flag::reset_on_fork, "SCHED_FLAG_RESET_ON_FORK" },
                            { scheduler::flag::reclaim, "SCHED_FLAG_RECLAIM" },
                            { scheduler::flag::dl_overrun, "SCHED_FLAG_DL_OVERRUN" },
                            { scheduler::flag::keep_policy, "SCHED_FLAG_KEEP_POLICY" },
                            { scheduler::flag::keep_params, "SCHED_FLAG_KEEP_PARAMS" },
                            { scheduler::flag::util_clamp_min, "SCHED_FLAG_UTIL_CLAMP_MIN" },
                            { scheduler::flag::util_clamp_max, "SCHED_FLAG_UTIL_CLAMP_MAX" })

void from_json(const nlohmann::json &j, scheduler &v);

void validate(const scheduler &v);

} // namespace linyaps_box::config
