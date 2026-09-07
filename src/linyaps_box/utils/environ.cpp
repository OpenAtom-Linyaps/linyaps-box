// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/environ.h"

namespace linyaps_box::utils {

auto is_invalid_env(std::string_view env) noexcept -> bool
{
    auto pos = env.find('=');
    if (pos == std::string_view::npos) {
        return true;
    }

    if (pos == 0) {
        return true;
    }

    if (env.find('\0') != std::string_view::npos) {
        return true;
    }

    return false;
}

} // namespace linyaps_box::utils
