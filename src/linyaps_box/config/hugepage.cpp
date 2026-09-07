// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hugepage.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, hugepage_limit &v)
{
    j.at("pageSize").get_to(v.page_size);
    j.at("limit").get_to(v.limit);
}

} // namespace linyaps_box::config
