// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hook.h"

#include "linyaps_box/utils/environ.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, hook &v)
{
    j.at("path").get_to(v.path);

    if (auto it = j.find("args"); it != j.end() && !it->is_null()) {
        it->get_to(v.args.emplace());
    }

    if (auto it = j.find("env"); it != j.end() && !it->is_null()) {
        it->get_to(v.env.emplace());
    }

    if (auto it = j.find("timeout"); it != j.end() && !it->is_null()) {
        it->get_to(v.timeout.emplace());
    }
}

void validate(std::string_view label, const hook &v)
{
    if (UNLIKELY(!v.path.is_absolute())) {
        throw std::runtime_error(fmt::format("{} hook path must be absolute", label));
    }

    if (v.env) {
        auto invalid = std::find_if(v.env->cbegin(), v.env->cend(), utils::is_invalid_env);
        if (UNLIKELY(invalid != v.env->cend())) {
            throw std::runtime_error(
              fmt::format("{} hook.env contains a invalid env: {}", label, *invalid));
        }
    }

    if (UNLIKELY(v.timeout && *v.timeout <= 0)) {
        throw std::runtime_error(fmt::format("{} hook timeout must be greater than zero", label));
    }
}

} // namespace linyaps_box::config
