// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/epoll.h>

#include <vector>

namespace linyaps_box::utils {

enum class epoll_operation : uint8_t {
    add = EPOLL_CTL_ADD,
    modify = EPOLL_CTL_MOD,
    remove = EPOLL_CTL_DEL
};

auto epoll_create1(int flags) -> linyaps_box::utils::file_descriptor;

[[nodiscard]] auto epoll_wait(const linyaps_box::utils::file_descriptor &efd,
                              std::vector<struct epoll_event> &events,
                              int timeout) -> uint;

auto epoll_ctl(const linyaps_box::utils::file_descriptor &efd,
               epoll_operation op,
               const linyaps_box::utils::file_descriptor &fd,
               struct epoll_event *event = nullptr) -> void;

} // namespace linyaps_box::utils
