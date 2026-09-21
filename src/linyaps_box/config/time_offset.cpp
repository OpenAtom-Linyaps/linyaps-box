// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/time_offset.h"

#include "linyaps_box/utils/strict_json.h"
#include "linyaps_box/utils/utils.h"

#include <stdexcept>
#include <string>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, time_offset &v)
{
    utils::require_object(j);
    if (auto it = j.find("secs"); it != j.end() && !it->is_null()) {
        it->get_to(v.secs.emplace());
    }

    if (auto it = j.find("nanosecs"); it != j.end() && !it->is_null()) {
        it->get_to(v.nanosecs.emplace());
    }
}

void validate(const time_offset &v)
{
    const uint32_t max_nanosecs{ 1'000'000'000 };
    if (UNLIKELY(v.nanosecs && *v.nanosecs >= max_nanosecs)) {
        throw std::runtime_error("timeOffsets nanosecs must be smaller than "
                                 + std::to_string(max_nanosecs));
    }
}

} // namespace linyaps_box::config
