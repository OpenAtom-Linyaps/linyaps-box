// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace linyaps_box::config {

struct exec_cpu_affinity
{
    std::optional<std::string> initial;
    std::optional<std::string> final;
};

void from_json(const nlohmann::json &j, exec_cpu_affinity &v);

} // namespace linyaps_box::config
