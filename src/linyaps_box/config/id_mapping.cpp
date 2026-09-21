// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/id_mapping.h"

#include "linyaps_box/utils/strict_json.h"

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, id_mapping &v)
{
    utils::require_object(j);
    j.at("hostID").get_to(v.host_id);
    j.at("containerID").get_to(v.container_id);
    j.at("size").get_to(v.size);
}

} // namespace linyaps_box::config
