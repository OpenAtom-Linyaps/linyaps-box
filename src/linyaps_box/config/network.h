// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

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

void validate(const network &v);

auto ops_of(json::object_tag<network>) -> const json::frame_ops &;

auto ops_of(json::object_tag<network::priority>) -> const json::frame_ops &;
} // namespace linyaps_box::config
