// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

#include <cstdint>

namespace linyaps_box::config {

struct id_mapping
{
    uint32_t host_id;
    uint32_t container_id;
    uint32_t size;
};

void from_json(const utils::strict_json &j, id_mapping &v);

} // namespace linyaps_box::config
