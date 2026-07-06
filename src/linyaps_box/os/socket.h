// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>

#include <sys/socket.h>

namespace linyaps_box::os {

// TODO: refactor with expected on later
struct recv_result
{
    ssize_t bytes;
    int error;

    explicit operator bool() const noexcept { return error == 0; }
};

struct socketpair_result
{
    int first{ -1 };
    int second{ -1 };
    int error{ 0 };

    explicit operator bool() const noexcept { return error == 0; }
};

auto sendmsg(int fd, const struct msghdr *msg, int flags) -> int;
auto send(int fd, const void *buf, std::size_t len, int flags) -> int;
auto connect(int fd, const struct sockaddr *addr, socklen_t addrlen) -> int;

auto recvmsg(int fd, struct msghdr *msg, int flags) -> recv_result;
auto recv(int fd, void *buf, std::size_t len, int flags) -> recv_result;

auto socketpair(int domain, int type, int protocol) -> socketpair_result;

} // namespace linyaps_box::os
