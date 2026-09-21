// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/personality.h"

#include "linyaps_box/utils/strict_json.h"

#include <fmt/format.h>

#include <stdexcept>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, personality &v)
{
    utils::require_object(j);
    const auto domain_name = j.at("domain").get<std::string_view>();
    const auto domain_opt = utils::enum_table_v<personality::domain>.from_name(domain_name);
    if (UNLIKELY(!domain_opt)) {
        throw std::runtime_error(fmt::format("unknown personality domain: {}", domain_name));
    }
    v.domain_ = *domain_opt;

    if (const auto flags_it = j.find("flags"); flags_it != j.cend() && !flags_it->is_null()) {
        flags_it->get_to(v.flags.emplace());
    }
}

} // namespace linyaps_box::config
