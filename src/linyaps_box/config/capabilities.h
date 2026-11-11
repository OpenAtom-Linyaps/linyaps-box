// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct capabilities
{
    std::optional<std::vector<std::string>> effective;
    std::optional<std::vector<std::string>> bounding;
    std::optional<std::vector<std::string>> inheritable;
    std::optional<std::vector<std::string>> permitted;
    std::optional<std::vector<std::string>> ambient;
};

auto ops_of(json::object_tag<capabilities>) -> const json::frame_ops &;
} // namespace linyaps_box::config
