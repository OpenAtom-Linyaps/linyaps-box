// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/network_device.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, network_device &v)
{
    if (auto it = j.find("name"); it != j.end() && !it->is_null()) {
        it->get_to(v.name.emplace());
    }
}

} // namespace linyaps_box::config
