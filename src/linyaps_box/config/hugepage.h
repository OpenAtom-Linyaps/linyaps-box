// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <string>

namespace linyaps_box::config {

struct hugepage_limit
{
    std::string page_size;
    uint64_t limit;
};

void from_json(const nlohmann::json &j, hugepage_limit &v);

} // namespace linyaps_box::config
