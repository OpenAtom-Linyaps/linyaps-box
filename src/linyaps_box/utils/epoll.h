// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/epoll.h>

namespace linyaps_box::utils {

enum class epoll_operation : uint8_t {
    add = EPOLL_CTL_ADD,
    modify = EPOLL_CTL_MOD,
    remove = EPOLL_CTL_DEL
};

auto epoll_create1(int flags) -> file_descriptor;

[[nodiscard]] auto epoll_wait(const file_descriptor &efd,
                              struct epoll_event *events,
                              std::size_t maxevents,
                              int timeout) -> uint;

auto epoll_ctl(const file_descriptor &efd,
               epoll_operation op,
               const file_descriptor &fd,
               struct epoll_event *event = nullptr) -> void;

} // namespace linyaps_box::utils
