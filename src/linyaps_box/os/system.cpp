// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/system.h"

#include "linyaps_box/utils/utils.h"

#include <unistd.h>

namespace linyaps_box::os {

auto sethostname(std::string_view name) noexcept -> os::Result<void>
{
    if (UNLIKELY(::sethostname(name.data(), name.size()) == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto setdomainname(std::string_view name) noexcept -> os::Result<void>
{
    if (UNLIKELY(::setdomainname(name.data(), name.size()) == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

} // namespace linyaps_box::os
