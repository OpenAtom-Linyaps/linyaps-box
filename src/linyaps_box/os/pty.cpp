// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/pty.h"

#include "linyaps_box/utils/utils.h"

namespace linyaps_box::os {
auto unlockpt(utils::file_descriptor_ref fd) noexcept -> Result<void>
{
    if (UNLIKELY(::unlockpt(fd) < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto ptsname(utils::file_descriptor_ref fd, utils::span<char> buf) noexcept
  -> Result<std::string_view>
{
    auto ret = ::ptsname_r(fd, buf.data(), buf.size());
    if (LIKELY(ret == 0)) {
        return std::string_view{ buf.data() };
    }

    return unexpected{ make_error_code(errno) };
}
} // namespace linyaps_box::os
