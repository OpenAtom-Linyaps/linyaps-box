// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/id_mapping.h"

#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(id_mapping,
                          id_mapping_schema,
                          "id_mapping",
                          field<&id_mapping::host_id>("hostID").required(),
                          field<&id_mapping::container_id>("containerID").required(),
                          field<&id_mapping::size>("size").required())

} // namespace linyaps_box::config
