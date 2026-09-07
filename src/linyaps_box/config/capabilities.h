// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

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

void from_json(const nlohmann::json &j, capabilities &v);

} // namespace linyaps_box::config
