// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/ranges.h>
#include <sys/prctl.h>

namespace linyaps_box::os {

[[nodiscard]] auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>;

template <typename... Args>
[[nodiscard]] auto prctl(int option, Args... args) noexcept -> Result<int>
{
    auto ret = ::prctl(option, std::forward<Args>(args)...);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return ret;
}

auto get_exit_code(int status) noexcept -> Result<int>;

} // namespace linyaps_box::os
