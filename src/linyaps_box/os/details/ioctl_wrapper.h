// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/utils.h"

#include <sys/ioctl.h>

namespace linyaps_box::os::details {
template <typename Arg = uintptr_t>
auto ioctl(int fd, unsigned long cmd, Arg arg = 0) noexcept -> Result<int>
{
    uintptr_t raw_arg; // NOLINT
    if constexpr (std::is_pointer_v<Arg>) {
        raw_arg = reinterpret_cast<uintptr_t>(arg); // NOLINT
    } else {
        raw_arg = static_cast<uintptr_t>(arg);
    }

    int ret = ::ioctl(fd, cmd, raw_arg);
    if (UNLIKELY(ret < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return ret;
}
} // namespace linyaps_box::os::details
