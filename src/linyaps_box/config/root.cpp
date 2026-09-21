// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/root.h"

#include "linyaps_box/utils/strict_json.h"
#include "linyaps_box/utils/utils.h"

#include <stdexcept>

namespace linyaps_box::config {

void from_json(const utils::strict_json &j, root &v)
{
    utils::require_object(j);
    j.at("path").get_to(v.path);
    v.readonly = j.value("readonly", false);
}

void validate(const root &v)
{
    if (UNLIKELY(v.path.empty())) {
        throw std::runtime_error("root.path must not be empty");
    }
}

} // namespace linyaps_box::config
