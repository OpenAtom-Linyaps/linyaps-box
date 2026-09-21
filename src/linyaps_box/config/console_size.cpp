// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/console_size.h"

#include "linyaps_box/utils/strict_json.h"

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, console_size &v)
{
    utils::require_object(j);
    j.at("height").get_to(v.height);
    j.at("width").get_to(v.width);
}

} // namespace linyaps_box::config
