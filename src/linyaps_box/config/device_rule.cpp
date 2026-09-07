// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/device_rule.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, device_rule &v)
{
    bool have_allow{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "allow")) {
            val.get_to(v.allow);
            have_allow = true;
        } else if (key_matches(k, "type")) {
            if (!val.is_null()) {
                auto type_name = val.get<std::string_view>();
                auto type_opt = get_enum_table_from<device_rule::type>().from_name(type_name);
                if (UNLIKELY(!type_opt)) {
                    throw std::runtime_error(
                      fmt::format("device_rule.type must be one of a/c/b: {}", type_name));
                }

                v.type_ = *type_opt;
            }
        } else if (key_matches(k, "major")) {
            if (!val.is_null()) {
                val.get_to(v.major.emplace());
            }
        } else if (key_matches(k, "minor")) {
            if (!val.is_null()) {
                val.get_to(v.minor.emplace());
            }
        } else if (key_matches(k, "access")) {
            if (!val.is_null()) {
                auto access_str = val.get_ref<const std::string &>();
                auto access_flags{ device_rule::access_flag::none };
                for (const auto c : access_str) {
                    auto flag_opt = get_enum_table_from<device_rule::access_flag>().from_name(
                      std::string_view(&c, 1));
                    if (UNLIKELY(!flag_opt)) {
                        throw std::runtime_error(
                          fmt::format("device_rule.access must be a composition of r/w/m: {}", c));
                    }

                    access_flags |= *flag_opt;
                }

                v.access = access_flags;
            }
        }
    }

    if (UNLIKELY(!have_allow)) {
        throw std::runtime_error("device_rule.allow is required");
    }
}

} // namespace linyaps_box::config
