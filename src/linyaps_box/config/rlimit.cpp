// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/rlimit.h"

#include "linyaps_box/utils/strict_json.h"

#include <fmt/format.h>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, rlimit &v)
{
    utils::require_object(j);
    auto name = j.at("type").get<std::string_view>();
    auto opt = utils::enum_table_v<rlimit::type>.from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error(fmt::format("unknown rlimit type: {}", name));
    }

    v.type_ = *opt;
    j.at("soft").get_to(v.soft);
    j.at("hard").get_to(v.hard);
}

} // namespace linyaps_box::config
