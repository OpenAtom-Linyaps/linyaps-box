// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/specconv/capabilities.h"

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

namespace linyaps_box::specconv {

auto to_capability(const std::string &name) noexcept -> std::optional<int>
{
#ifdef LINYAPS_BOX_ENABLE_CAP
    cap_value_t value{ };
    if (cap_from_name(name.c_str(), &value) < 0) {
        return std::nullopt;
    }

    return static_cast<int>(value);
#else
    return std::nullopt;
#endif
}

} // namespace linyaps_box::specconv
