// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct intel_rdt
{
    std::optional<std::string> clos_id;
    std::optional<std::string> l3_cache_schema;
    std::optional<std::string> memory_bandwidth_schema;
    std::optional<std::vector<std::string>> schemata;
    std::optional<bool> enable_monitoring;
};

void validate(const intel_rdt &v);

auto ops_of(json::object_tag<intel_rdt>) -> const json::frame_ops &;
} // namespace linyaps_box::config
