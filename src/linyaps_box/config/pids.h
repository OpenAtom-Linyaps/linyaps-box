// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct pids
{
    std::optional<int64_t> limit;
};

void from_json(const nlohmann::json &j, pids &v);

} // namespace linyaps_box::config
