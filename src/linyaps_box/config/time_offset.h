// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>

namespace linyaps_box::config {

struct time_offset
{
    int64_t secs;
    uint32_t nanosecs;
};

void from_json(const nlohmann::json &j, time_offset &v);

} // namespace linyaps_box::config
