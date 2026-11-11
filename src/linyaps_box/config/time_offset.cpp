// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/time_offset.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(time_offset,
                          time_offset_schema,
                          "time_offset",
                          field<&time_offset::secs>("secs").ignore_null(),
                          field<&time_offset::nanosecs>("nanosecs").ignore_null())

void validate(const time_offset &v)
{
    const uint32_t max_nanosecs{ 1'000'000'000 };
    require(!v.nanosecs || *v.nanosecs < max_nanosecs,
            "timeOffsets nanosecs must be smaller than {}",
            max_nanosecs);
}

} // namespace linyaps_box::config
