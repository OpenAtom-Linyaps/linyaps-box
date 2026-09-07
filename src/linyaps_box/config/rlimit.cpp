// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/rlimit.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, rlimit &v)
{
    auto name = j.at("type").get<std::string_view>();
    auto opt = get_enum_table_from<rlimit::type>().from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error(fmt::format("unknown rlimit type: {}", name));
    }

    v.type_ = *opt;
    j.at("soft").get_to(v.soft);
    j.at("hard").get_to(v.hard);
}

} // namespace linyaps_box::config
