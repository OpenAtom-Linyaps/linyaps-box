// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct network
{
    struct priority
    {
        std::string name;
        uint32_t priority;
    };

    std::optional<uint32_t> class_id;
    std::optional<std::vector<priority>> priorities;
};

void from_json(const nlohmann::json &j, network::priority &v);

void from_json(const nlohmann::json &j, network &v);

void validate(const network &v);

} // namespace linyaps_box::config
