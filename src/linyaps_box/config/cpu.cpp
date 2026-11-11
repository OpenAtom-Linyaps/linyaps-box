// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/cpu.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <cstdint>

namespace linyaps_box::config {

namespace {

auto read_idle(const json::detail::value_view &value) -> cpu::idle
{
    const auto idle = json::detail::number_from<std::int64_t>(value);
    if (UNLIKELY(idle != 0 && idle != 1)) {
        throw json::decode_error{ "cpu.idle must be 0 or 1" };
    }

    return idle == 1 ? cpu::idle::idle : cpu::idle::none;
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(cpu,
                          cpu_schema,
                          "cpu",
                          field<&cpu::shares>("shares").ignore_null(),
                          field<&cpu::quota>("quota").ignore_null(),
                          field<&cpu::burst>("burst").ignore_null(),
                          field<&cpu::period>("period").ignore_null(),
                          field<&cpu::realtime_runtime>("realtimeRuntime").ignore_null(),
                          field<&cpu::realtime_period>("realtimePeriod").ignore_null(),
                          field<&cpu::cpus>("cpus").ignore_null(),
                          field<&cpu::mems>("mems").ignore_null(),
                          field<&cpu::idle_, &read_idle>("idle").ignore_null())

void validate(const cpu &v)
{
    require(!v.quota || *v.quota <= 0 || !v.burst || *v.burst <= static_cast<uint64_t>(*v.quota),
            "cpu.quota must be no smaller than cpu.burst");
}

} // namespace linyaps_box::config
