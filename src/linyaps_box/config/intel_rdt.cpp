// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/intel_rdt.h"

#include "linyaps_box/config/utils.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, intel_rdt &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "closID")) {
            if (!val.is_null()) {
                val.get_to(v.clos_id.emplace());
            }
        } else if (key_matches(k, "l3CacheSchema")) {
            if (!val.is_null()) {
                val.get_to(v.l3_cache_schema.emplace());
            }
        } else if (key_matches(k, "memBwSchema")) {
            if (!val.is_null()) {
                val.get_to(v.memory_bandwidth_schema.emplace());
            }
        } else if (key_matches(k, "schemata")) {
            if (!val.is_null()) {
                val.get_to(v.schemata.emplace());
            }
        } else if (key_matches(k, "enableMonitoring")) {
            if (!val.is_null()) {
                val.get_to(v.enable_monitoring.emplace());
            }
        }
    }
}

} // namespace linyaps_box::config
