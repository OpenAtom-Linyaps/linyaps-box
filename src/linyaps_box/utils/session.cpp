// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/session.h"

#include <system_error>

namespace linyaps_box::utils {

auto setsid() -> pid_t
{
    auto ret = ::setsid();
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "setsid");
    }

    return ret;
}

} // namespace linyaps_box::utils
