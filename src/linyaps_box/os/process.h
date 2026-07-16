// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"

#include <fmt/ranges.h>
#include <sys/prctl.h>

namespace linyaps_box::os {

[[nodiscard]] auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>;

template <typename... Args>
[[nodiscard]] auto prctl(int option, Args... args) noexcept -> Result<int>
{
    auto ret = ::prctl(option, std::forward<Args>(args)...);
    if (ret < 0) {
        auto msg = fmt::format("prctl op {} with args: [{}]",
                               option,
                               fmt::join(std::forward_as_tuple(std::forward<Args>(args)...), ", "));

        return unexpected(Err{ msg, errno });
    }

    return ret;
}

auto get_exit_code(int status) noexcept -> Result<int>;

} // namespace linyaps_box::os
