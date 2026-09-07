// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

namespace linyaps_box::config {

struct console_size
{
    unsigned short height;
    unsigned short width;
};

void from_json(const nlohmann::json &j, console_size &v);

} // namespace linyaps_box::config
