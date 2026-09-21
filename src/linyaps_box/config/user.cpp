// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/user.h"

#include "linyaps_box/utils/strict_json.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, user &v)
{
    utils::require_object(j);
    j.at("uid").get_to(v.uid);
    j.at("gid").get_to(v.gid);

    if (auto it = j.find("umask"); it != j.end() && !it->is_null()) {
        v.umask.emplace() = static_cast<std::filesystem::perms>(
          utils::detail::get_number<std::underlying_type_t<std::filesystem::perms>>(*it));
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
