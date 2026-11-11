// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <optional>
#include <string>

namespace linyaps_box::config {

struct network_device
{
    std::optional<std::string> name;
};

auto ops_of(json::object_tag<network_device>) -> const json::frame_ops &;
} // namespace linyaps_box::config
