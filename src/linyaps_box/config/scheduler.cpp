// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/scheduler.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/format.h>

#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(scheduler,
                          scheduler_schema,
                          "scheduler",
                          field<&scheduler::policy_>("policy").required(),
                          field<&scheduler::nice>("nice").ignore_null(),
                          field<&scheduler::priority>("priority").ignore_null(),
                          field<&scheduler::flags>("flags").ignore_null(),
                          field<&scheduler::runtime>("runtime").ignore_null(),
                          field<&scheduler::deadline>("deadline").ignore_null(),
                          field<&scheduler::period>("period").ignore_null())

void validate(const scheduler &v)
{
    using policy_t = scheduler::policy;

    require(!v.nice || v.policy_ == policy_t::other || v.policy_ == policy_t::batch,
            "scheduler.nice can only be specified for SCHED_OTHER or SCHED_BATCH");
    if (v.nice) {
        require(*v.nice >= -20 && *v.nice <= 19,
                "scheduler.nice must be in range [-20, 19]: got {}",
                *v.nice);
    }

    require(!v.priority || *v.priority == 0 || v.policy_ == policy_t::fifo
              || v.policy_ == policy_t::rr,
            "scheduler.priority can only be specified for SCHED_FIFO or SCHED_RR");
    require((!v.runtime && !v.deadline && !v.period) || v.policy_ == policy_t::deadline,
            "scheduler runtime/deadline/period can only be specified for SCHED_DEADLINE");
}

} // namespace linyaps_box::config
