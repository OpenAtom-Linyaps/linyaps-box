// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

#include <optional>
#include <string>

namespace linyaps_box::config {

struct network_device
{
    std::optional<std::string> name;
};

void from_json(const utils::strict_json &j, network_device &v);

} // namespace linyaps_box::config
