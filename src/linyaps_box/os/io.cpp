// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/io.h"

#include "linyaps_box/utils/utils.h"

namespace linyaps_box::os {
auto read(utils::file_descriptor_ref fd, utils::span<std::byte> buf) noexcept -> Result<std::size_t>
{
    while (true) {
        auto ret = ::read(fd, buf.data(), buf.size());
        if (LIKELY(ret >= 0)) {
            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}

auto write(utils::file_descriptor_ref fd, utils::span<const std::byte> buf) noexcept
  -> Result<std::size_t>
{
    while (true) {
        auto ret = ::write(fd, buf.data(), buf.size());
        if (LIKELY(ret >= 0)) {
            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}
} // namespace linyaps_box::os
