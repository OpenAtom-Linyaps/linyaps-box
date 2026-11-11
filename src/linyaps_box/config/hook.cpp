// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hook.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/environ.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(hook,
                          hook_schema,
                          "hook",
                          field<&hook::path>("path").required(),
                          field<&hook::args>("args").ignore_null(),
                          field<&hook::env>("env").ignore_null(),
                          field<&hook::timeout>("timeout").ignore_null())

void validate(std::string_view label, const hook &v)
{
    require(v.path.is_absolute(), "{} hook path must be absolute", label);

    if (v.env) {
        const auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), utils::is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error(
              fmt::format("{} hook.env contains a invalid env: {}", label, *invalid));
        }
    }

    require(!v.timeout || *v.timeout > 0, "{} hook timeout must be greater than zero", label);
}

} // namespace linyaps_box::config
