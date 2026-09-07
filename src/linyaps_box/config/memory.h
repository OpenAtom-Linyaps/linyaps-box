// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct memory
{
    std::optional<int64_t> limit;
    std::optional<int64_t> reservation;
    std::optional<int64_t> swap;
    std::optional<int64_t> kernel;
    std::optional<int64_t> kernel_tcp;
    std::optional<uint64_t> swappiness;
    std::optional<bool> disable_OOM_killer;
    std::optional<bool> use_hierarchy;
    std::optional<bool> check_before_update;
};

void from_json(const nlohmann::json &j, memory &v);

void validate(const memory &v);

} // namespace linyaps_box::config
