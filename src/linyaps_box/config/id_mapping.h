// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>

namespace linyaps_box::config {

struct id_mapping
{
    uint32_t host_id;
    uint32_t container_id;
    uint32_t size;
};

void from_json(const nlohmann::json &j, id_mapping &v);

} // namespace linyaps_box::config
