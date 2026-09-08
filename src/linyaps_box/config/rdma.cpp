// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/rdma.h"

#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, rdma &v)
{
    if (auto it = j.find("hcaHandles"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_handles.emplace());
    }

    if (auto it = j.find("hcaObjects"); it != j.end() && !it->is_null()) {
        it->get_to(v.hca_objects.emplace());
    }
}

void validate(const rdma &v)
{
    if (UNLIKELY(!v.hca_handles && !v.hca_objects)) {
        throw std::runtime_error("rdma requires at least one of hcaHandles or hcaObjects");
    }
}

} // namespace linyaps_box::config
