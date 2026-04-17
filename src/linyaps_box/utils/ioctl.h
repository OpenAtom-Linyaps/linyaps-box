// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/utils.h"

#include <sys/ioctl.h>

namespace linyaps_box::utils {

using request_t = extract_t<decltype(get_n_params_type<1>(::ioctl))>;

template<typename... Args>
[[nodiscard]] auto ioctl(const file_descriptor &fd, request_t request, Args... args) -> int
{
    auto ret = ::ioctl(fd.get(), request, std::forward<Args>(args)...);
    if (ret != 0) {
        auto msg =
                "ioctl fd " + std::to_string(fd.get()) + ", request op " + std::to_string(request);
        bool first = true;
        ((msg += (first ? "" : ", ") + stringify_arg(args), first = false), ...);
        msg.push_back(']');

        throw std::system_error(errno, std::system_category(), msg);
    }

    return ret;
}

} // namespace linyaps_box::utils
