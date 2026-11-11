// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct pids
{
    std::optional<int64_t> limit;
};

auto ops_of(json::object_tag<pids>) -> const json::frame_ops &;
} // namespace linyaps_box::config
