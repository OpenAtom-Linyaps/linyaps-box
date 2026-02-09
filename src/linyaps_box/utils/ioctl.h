// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/utils.h"

#include <sys/ioctl.h>

namespace linyaps_box::utils {

template<typename... Args>
auto ioctl(const file_descriptor &fd,
           decltype(get_n_params_type<1>(ioctl))::type request,
           Args... args) -> unsigned int
{
    auto ret = ::ioctl(fd.get(), request, std::forward<Args>(args)...);
    if (ret != 0) {
        throw std::system_error(errno, std::system_category(), "ioctl");
    }

    return ret;
}

} // namespace linyaps_box::utils
