// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "utils.h"

#include <sys/prctl.h>

#include <cstdint>
#include <system_error>
#include <utility>

#include <sys/types.h>
#include <unistd.h>

namespace linyaps_box::utils {

enum class WaitStatus : uint8_t { Reaped, None, NoChild };

struct WaitResult
{
    WaitStatus status{ WaitStatus::None };
    pid_t pid{ -1 };
    int exit_code{ -1 };
};

auto waitpid(pid_t pid, int options) -> WaitResult;

template<typename... Args>
[[nodiscard]] auto prctl(int option, Args... args) -> int
{
    auto ret = ::prctl(option, std::forward<Args>(args)...);
    if (ret < 0) {
        auto msg = "prctl op " + std::to_string(option) + " with args: [";

        bool first = true;
        ((msg += (first ? "" : ", ") + stringify_arg(args), first = false), ...);
        msg.push_back(']');

        throw std::system_error(errno, std::system_category(), msg);
    }

    return ret;
}

} // namespace linyaps_box::utils
