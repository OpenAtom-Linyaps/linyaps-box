// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/intel_rdt.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
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

void validate(const intel_rdt &v)
{
    if (v.memory_bandwidth_schema) {
        const auto &s = *v.memory_bandwidth_schema;
        if (UNLIKELY(s.rfind("MB:", 0) != 0 || s.find('\n') != std::string::npos)) {
            throw std::runtime_error(
              fmt::format("intelRdt.memBwSchema must match ^MB:[^\\n]*$: {}", s));
        }
    }

    if (v.schemata) {
        std::for_each(v.schemata->cbegin(), v.schemata->cend(), [](std::string_view line) {
            if (UNLIKELY(line.find('\n') != std::string_view::npos)) {
                throw std::runtime_error("intelRdt.schemata entries must not contain newlines");
            }
        });
    }
}

} // namespace linyaps_box::config
