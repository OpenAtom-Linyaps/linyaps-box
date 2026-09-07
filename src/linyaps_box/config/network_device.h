// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace linyaps_box::config {

struct network_device
{
    std::optional<std::string> name;
};

void from_json(const nlohmann::json &j, network_device &v);

} // namespace linyaps_box::config
