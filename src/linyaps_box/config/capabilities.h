// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

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

void from_json(const utils::strict_json &j, capabilities &v);

} // namespace linyaps_box::config
