// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/time_offset.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, time_offset &v)
{
    if (auto it = j.find("secs"); it != j.end() && !it->is_null()) {
        it->get_to(v.secs.emplace());
    }

    if (auto it = j.find("nanosecs"); it != j.end() && !it->is_null()) {
        it->get_to(v.nanosecs.emplace());
    }
}

} // namespace linyaps_box::config
