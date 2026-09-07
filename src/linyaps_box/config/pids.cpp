// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/pids.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, pids &v)
{
    if (auto it = j.find("limit"); it != j.end() && !it->is_null()) {
        it->get_to(v.limit.emplace());
    }
}

} // namespace linyaps_box::config
