// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/tty.h"

#include <cerrno>

#include <unistd.h>

namespace linyaps_box::os {
auto isatty(const utils::file_descriptor &fd) noexcept -> Result<bool>
{
    auto ret = ::isatty(fd.get());
    if (ret == 1) {
        return true;
    }

    if (errno == ENOTTY || errno == EINVAL) {
        return false;
    }

    return unexpected(make_error_code(errno));
}

} // namespace linyaps_box::os
