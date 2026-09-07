// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <filesystem>

namespace linyaps_box::config {

struct root
{
    std::filesystem::path path;
    bool readonly{ false };
};

void from_json(const nlohmann::json &j, root &v);

void validate(const root &v);

} // namespace linyaps_box::config
