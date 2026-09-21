// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct time_offset
{
    std::optional<int64_t> secs;
    std::optional<uint32_t> nanosecs;
};

void from_json(const utils::strict_json &j, time_offset &v);

void validate(const time_offset &v);

} // namespace linyaps_box::config
