// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/socket.h"

#include "linyaps_box/utils/utils.h"

#include <array>
#include <cerrno>

namespace linyaps_box::os {

auto sendmsg(int fd, const struct msghdr *msg, int flags) -> int
{
    while (true) {
        auto ret = ::sendmsg(fd, msg, flags);
        if (LIKELY(ret >= 0)) {
            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        return errno;
    }
}

auto send(int fd, const void *buf, std::size_t len, int flags) -> int
{
    while (true) {
        auto ret = ::send(fd, buf, len, flags);
        if (LIKELY(ret >= 0)) {
            // SOCK_SEQPACKET guarantees atomic send: ret >= 0 implies ret == len.
            // A partial send would break message boundaries and indicates misuse
            // (e.g. on a stream socket)
            if (UNLIKELY(static_cast<std::size_t>(ret) != len)) {
                return EIO;
            }

            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        return errno;
    }
}

auto recvmsg(int fd, struct msghdr *msg, int flags) -> recv_result
{
    while (true) {
        auto len = ::recvmsg(fd, msg, flags);
        if (len >= 0) {
            return recv_result{ len, 0 };
        }

        if (errno == EINTR) {
            continue;
        }

        return recv_result{ -1, errno };
    }
}

auto recv(int fd, void *buf, std::size_t len, int flags) -> recv_result
{
    while (true) {
        auto ret = ::recv(fd, buf, len, flags);
        if (ret >= 0) {
            return recv_result{ ret, 0 };
        }

        if (errno == EINTR) {
            continue;
        }

        return recv_result{ -1, errno };
    }
}

auto socketpair(int domain, int type, int protocol) -> socketpair_result
{
    std::array<int, 2> fds{ };
    if (UNLIKELY(::socketpair(domain, type, protocol, fds.data()) == -1)) {
        return socketpair_result{ -1, -1, errno };
    }

    return socketpair_result{ fds[0], fds[1], 0 };
}

auto connect(int fd, const struct sockaddr *addr, socklen_t addrlen) -> int
{
    while (true) {
        if (LIKELY(::connect(fd, addr, addrlen) == 0)) {
            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        return errno;
    }
}

} // namespace linyaps_box::os
