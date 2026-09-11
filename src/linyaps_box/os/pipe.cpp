// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/pipe.h"

namespace linyaps_box::os {

[[nodiscard]] auto pipe2(utils::bitflags<sys::pipe_flags> flags) noexcept
  -> Result<std::pair<utils::file_descriptor, utils::file_descriptor>>
{
    std::array<int, 2> fds; // NOLINT
    if (::pipe2(fds.data(), static_cast<int>(flags.to_raw())) == -1) {
        return unexpected{ make_error_code(errno) };
    }

    return std::make_pair(utils::file_descriptor(fds[0]), utils::file_descriptor(fds[1]));
}

} // namespace linyaps_box::os
