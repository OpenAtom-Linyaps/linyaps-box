// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct hook
{
    std::filesystem::path path;
    std::optional<std::vector<std::string>> args;
    std::optional<std::vector<std::string>> env;
    std::optional<int> timeout;
};

void from_json(const utils::strict_json &j, hook &v);

void validate(std::string_view label, const hook &v);

} // namespace linyaps_box::config
