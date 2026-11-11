// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/intel_rdt.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/log/macro.h"

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(intel_rdt,
                          intel_rdt_schema,
                          "intel_rdt",
                          field<&intel_rdt::clos_id>("closID").ignore_null(),
                          field<&intel_rdt::l3_cache_schema>("l3CacheSchema").ignore_null(),
                          field<&intel_rdt::memory_bandwidth_schema>("memBwSchema").ignore_null(),
                          field<&intel_rdt::schemata>("schemata").ignore_null(),
                          field<&intel_rdt::enable_monitoring>("enableMonitoring").ignore_null())

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
