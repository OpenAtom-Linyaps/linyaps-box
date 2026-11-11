// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hooks.h"

#include "linyaps_box/json/parser.h"

#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(hooks,
                          hooks_schema,
                          "hooks",
                          field<&hooks::prestart>("prestart").ignore_null(),
                          field<&hooks::create_runtime>("createRuntime").ignore_null(),
                          field<&hooks::create_container>("createContainer").ignore_null(),
                          field<&hooks::start_container>("startContainer").ignore_null(),
                          field<&hooks::poststart>("poststart").ignore_null(),
                          field<&hooks::poststop>("poststop").ignore_null())

void validate(const hooks &v)
{
    const auto validate_hooks = [](std::string_view label, const auto &hooks) {
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
