// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/console_size.h"

#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, console_size &v)
{
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

} // namespace linyaps_box::config
