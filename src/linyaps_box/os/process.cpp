// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/process.h"

#include <sys/wait.h>

namespace linyaps_box::os {
auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>
{
    status = 0;
    while (true) {
        auto ret = ::waitpid(pid, &status, options);
        if (ret >= 0) {
            if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
                continue;
            }

            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }

    __builtin_unreachable();
}

auto get_exit_code(int status) noexcept -> Result<int>
{
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return unexpected(std::make_error_code(std::errc::invalid_argument));
}

} // namespace linyaps_box::os
