// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

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

void from_json(const nlohmann::json &j, intel_rdt &v);

} // namespace linyaps_box::config
