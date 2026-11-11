// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <cstdint>

namespace linyaps_box::config {

struct id_mapping
{
    uint32_t host_id;
    uint32_t container_id;
    uint32_t size;
};

auto ops_of(json::object_tag<id_mapping>) -> const json::frame_ops &;
} // namespace linyaps_box::config
