// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/socketpair.h"

#include <array>
#include <system_error>

namespace linyaps_box::utils {

auto socketpair(int domain, int type, int protocol) -> std::pair<file_descriptor, file_descriptor>
{
    std::array<int, 2> fds{};
    if (::socketpair(domain, type, protocol, fds.data()) == -1) {
        throw std::system_error(errno,
                                std::system_category(),
                                "socketpair(" + std::to_string(domain) + ", " + std::to_string(type)
                                        + ", " + std::to_string(protocol) + ")");
    }

    return std::make_pair(file_descriptor(fds[0]), file_descriptor(fds[1]));
}

} // namespace linyaps_box::utils
