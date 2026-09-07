// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/network.h"

#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, network::priority &v)
{
    j.at("name").get_to(v.name);
    j.at("priority").get_to(v.priority);
}

void from_json(const nlohmann::json &j, network &v)
{
    if (auto it = j.find("classID"); it != j.end() && !it->is_null()) {
        it->get_to(v.class_id.emplace());
    }

    if (auto it = j.find("priorities"); it != j.end() && !it->is_null()) {
        it->get_to(v.priorities.emplace());
    }
}

void validate(const network &v)
{
    if (!v.priorities) {
        return;
    }

    std::for_each(v.priorities->cbegin(), v.priorities->cend(), [](const auto &p) {
        if (UNLIKELY(p.name.empty())) {
            throw std::runtime_error("network.priorities name must not be empty");
        }
    });
}

} // namespace linyaps_box::config
