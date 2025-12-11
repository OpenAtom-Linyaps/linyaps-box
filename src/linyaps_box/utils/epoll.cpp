// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/epoll.h"

namespace linyaps_box::utils {

auto epoll_create1(int flags) -> linyaps_box::utils::file_descriptor
{
    auto ret = ::epoll_create1(flags);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "epoll_create1");
    }

    return linyaps_box::utils::file_descriptor{ ret };
}

auto epoll_wait(const linyaps_box::utils::file_descriptor &efd,
                std::vector<struct epoll_event> &events,
                int timeout) -> uint
{
    while (true) {
        auto ret = ::epoll_wait(efd.get(), events.data(), static_cast<int>(events.size()), timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(errno, std::system_category(), "epoll_wait");
        }

        return ret;
    }
}

auto epoll_ctl(const linyaps_box::utils::file_descriptor &efd,
               epoll_operation op,
               const linyaps_box::utils::file_descriptor &fd,
               struct epoll_event *event) -> void
{
    auto ret = ::epoll_ctl(efd.get(), static_cast<int>(op), fd.get(), event);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "epoll_ctl");
    }
}

} // namespace linyaps_box::utils
