// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/process.h"

#include <system_error>

#include <sys/wait.h>

namespace linyaps_box::utils {

auto waitpid(pid_t pid, int options) -> WaitResult
{
    int status{ 0 };
    while (true) {
        auto ret = ::waitpid(pid, &status, options);
        if (ret > 0) {
            return { WaitStatus::Reaped, ret, status };
        }

        if (ret == 0) { // fow WNOHANG
            return { WaitStatus::None };
        }

        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }

        if (errno == ECHILD) {
            return { WaitStatus::NoChild };
        }

        throw std::system_error(errno, std::system_category(), "waitpid");
    }
}

} // namespace linyaps_box::utils
