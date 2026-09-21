// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/capabilities.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/strict_json.h"

#include <string_view>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, capabilities &v)
{
    utils::require_object(j);
    auto parse_set = [](const utils::strict_json &j, std::vector<std::string> &set) {
        set.reserve(j.size());
        for (const auto &elem : j) {
            set.push_back(elem.get<std::string>());
        }
    };

    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "effective")) {
            if (!val.is_null()) {
                parse_set(val, v.effective.emplace());
            }
        } else if (key_matches(k, "ambient")) {
            if (!val.is_null()) {
                parse_set(val, v.ambient.emplace());
            }
        } else if (key_matches(k, "bounding")) {
            if (!val.is_null()) {
                parse_set(val, v.bounding.emplace());
            }
        } else if (key_matches(k, "inheritable")) {
            if (!val.is_null()) {
                parse_set(val, v.inheritable.emplace());
            }
        } else if (key_matches(k, "permitted")) {
            if (!val.is_null()) {
                parse_set(val, v.permitted.emplace());
            }
        }
    }
}

} // namespace linyaps_box::config
