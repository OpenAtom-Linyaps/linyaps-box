// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/personality.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace linyaps_box::config {

constexpr auto personality_domain_table = get_enum_table_from<personality::domain>();

void from_json(const nlohmann::json &j, personality &v)
{
    auto domain_name = j.at("domain").get<std::string_view>();
    auto domain_opt = personality_domain_table.from_name(domain_name);
    if (UNLIKELY(!domain_opt)) {
        throw std::runtime_error(fmt::format("unknown personality domain: {}", domain_name));
    }
    v.domain_ = *domain_opt;

    if (auto flags_it = j.find("flags"); flags_it != j.end() && !flags_it->is_null()) {
        flags_it->get_to(v.flags.emplace());
    }
}

} // namespace linyaps_box::config
