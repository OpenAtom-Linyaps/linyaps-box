// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/network.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <algorithm>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(network::priority,
                          network_priority_schema,
                          "network.priority",
                          field<&network::priority::name>("name").required(),
                          field<&network::priority::priority>("priority").required())

LINYAPS_BOX_CONFIG_SCHEMA(network,
                          network_schema,
                          "network",
                          field<&network::class_id>("classID").ignore_null(),
                          field<&network::priorities>("priorities").ignore_null())

void validate(const network &v)
{
    if (v.priorities) {
        std::for_each(v.priorities->cbegin(), v.priorities->cend(), [](const auto &p) {
            require(!p.name.empty(), "network.priorities name must not be empty");
        });
    }
}

} // namespace linyaps_box::config
