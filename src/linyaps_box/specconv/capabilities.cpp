// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/specconv/capabilities.h"

#include <sys/capability.h>

namespace linyaps_box::specconv {

auto to_capability(const std::string &name) noexcept -> std::optional<int>
{
    cap_value_t value{ };
    if (cap_from_name(name.c_str(), &value) < 0) {
        return std::nullopt;
    }

    return static_cast<int>(value);
}

} // namespace linyaps_box::specconv
