// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/time_offset.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, time_offset &v)
{
    j.at("secs").get_to(v.secs);
    j.at("nanosecs").get_to(v.nanosecs);
}

} // namespace linyaps_box::config
