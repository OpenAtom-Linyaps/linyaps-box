// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/exec_cpu_affinity.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, exec_cpu_affinity &v)
{
    if (auto it = j.find("initial"); it != j.end() && !it->is_null()) {
        it->get_to(v.initial.emplace());
    }

    if (auto it = j.find("final"); it != j.end() && !it->is_null()) {
        it->get_to(v.final.emplace());
    }
}

} // namespace linyaps_box::config
