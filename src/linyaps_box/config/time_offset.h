// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct time_offset
{
    std::optional<int64_t> secs;
    std::optional<uint32_t> nanosecs;
};

void validate(const time_offset &v);

auto ops_of(json::object_tag<time_offset>) -> const json::frame_ops &;
} // namespace linyaps_box::config
