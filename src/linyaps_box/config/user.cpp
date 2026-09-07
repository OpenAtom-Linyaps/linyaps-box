// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/user.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, user &v)
{
    j.at("uid").get_to(v.uid);
    j.at("gid").get_to(v.gid);

    if (auto it = j.find("umask"); it != j.end() && !it->is_null()) {
        it->get_to(v.umask.emplace());
    }

    if (auto it = j.find("additionalGids"); it != j.end() && !it->is_null()) {
        it->get_to(v.additional_gids.emplace());
    }
}

void validate(const user &v)
{
    if (!v.umask) {
        return;
    }

    auto val = v.umask.value();
    if (UNLIKELY((val & ~std::filesystem::perms::all) != std::filesystem::perms::none)) {
        throw std::runtime_error(
          fmt::format("user.umask is invalid: 0{:o}", static_cast<mode_t>(val)));
    }
}

} // namespace linyaps_box::config
