// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/network_device.h"

#include "linyaps_box/utils/strict_json.h"

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, network_device &v)
{
    utils::require_object(j);
    if (auto it = j.find("name"); it != j.end() && !it->is_null()) {
        it->get_to(v.name.emplace());
    }
}

} // namespace linyaps_box::config
