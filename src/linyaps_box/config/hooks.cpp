// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hooks.h"

#include "linyaps_box/config/utils.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, hooks &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "prestart")) {
            if (!val.is_null()) {
                val.get_to(v.prestart.emplace());
            }
        } else if (key_matches(k, "createRuntime")) {
            if (!val.is_null()) {
                val.get_to(v.create_runtime.emplace());
            }
        } else if (key_matches(k, "createContainer")) {
            if (!val.is_null()) {
                val.get_to(v.create_container.emplace());
            }
        } else if (key_matches(k, "startContainer")) {
            if (!val.is_null()) {
                val.get_to(v.start_container.emplace());
            }
        } else if (key_matches(k, "poststart")) {
            if (!val.is_null()) {
                val.get_to(v.poststart.emplace());
            }
        } else if (key_matches(k, "poststop")) {
            if (!val.is_null()) {
                val.get_to(v.poststop.emplace());
            }
        }
    }
}

void validate(const hooks &v)
{
    auto validate_hooks = [](std::string_view label, const auto &hooks) {
        for (const auto &h : hooks) {
            validate(label, h);
        }
    };

    if (v.prestart) {
        validate_hooks("prestart", *v.prestart);
    }

    if (v.create_runtime) {
        validate_hooks("create_runtime", *v.create_runtime);
    }

    if (v.create_container) {
        validate_hooks("create_container", *v.create_container);
    }

    if (v.start_container) {
        validate_hooks("start_container", *v.start_container);
    }

    if (v.poststart) {
        validate_hooks("poststart", *v.poststart);
    }

    if (v.poststop) {
        validate_hooks("poststop", *v.poststop);
    }
}

} // namespace linyaps_box::config
