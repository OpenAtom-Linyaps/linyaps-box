// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/cpu.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, cpu &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "shares")) {
            if (!val.is_null()) {
                val.get_to(v.shares.emplace());
            }
        } else if (key_matches(k, "quota")) {
            if (!val.is_null()) {
                val.get_to(v.quota.emplace());
            }
        } else if (key_matches(k, "burst")) {
            if (!val.is_null()) {
                val.get_to(v.burst.emplace());
            }
        } else if (key_matches(k, "period")) {
            if (!val.is_null()) {
                val.get_to(v.period.emplace());
            }
        } else if (key_matches(k, "realtimeRuntime")) {
            if (!val.is_null()) {
                val.get_to(v.realtime_runtime.emplace());
            }
        } else if (key_matches(k, "realtimePeriod")) {
            if (!val.is_null()) {
                val.get_to(v.realtime_period.emplace());
            }
        } else if (key_matches(k, "cpus")) {
            if (!val.is_null()) {
                v.cpus = parse_range_list(val.get<std::string_view>());
            }
        } else if (key_matches(k, "mems")) {
            if (!val.is_null()) {
                v.mems = parse_range_list(val.get<std::string_view>());
            }
        } else if (key_matches(k, "idle")) {
            if (!val.is_null()) {
                auto idle_val = val.get<int64_t>();
                if (UNLIKELY(idle_val != 0 && idle_val != 1)) {
                    throw std::runtime_error(fmt::format("cpu.idle must be 0 or 1: {}", idle_val));
                }

                v.idle_.emplace((idle_val == 1) ? cpu::idle::idle : cpu::idle::none);
            }
        }
    }
}

void validate(const cpu &v)
{
    if (UNLIKELY(v.quota && *v.quota > 0 && v.burst
                 && *v.burst > static_cast<uint64_t>(*v.quota))) {
        throw std::runtime_error("cpu.quota must be no smaller than cpu.burst");
    }
}

} // namespace linyaps_box::config
