// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/infra/unix_socket.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/socket.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iterator>
#include <stdexcept>

#include <sys/un.h>

namespace linyaps_box::infra {

unix_socket::unix_socket(utils::file_descriptor fd)
    : fd_(std::move(fd))
{
    if (UNLIKELY(fd_.nonblock())) {
        throw std::logic_error("unix_socket requires blocking mode");
    }
}

unix_socket unix_socket::connect(const std::filesystem::path &path)
{
    const auto &native = path.native();

    sockaddr_un addr{ };
    addr.sun_family = AF_UNIX;
    auto len = native.copy(static_cast<char *>(addr.sun_path), sizeof(addr.sun_path) - 1);
    if (UNLIKELY(len != native.size())) {
        throw std::system_error(ENAMETOOLONG, std::system_category(), "socket path too long");
    }

    addr.sun_path[len] = '\0';
    auto addr_len = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + len + 1);

    auto raw_fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (UNLIKELY(raw_fd == -1)) {
        throw std::system_error(errno, std::system_category(), "socket");
    }

    auto ret = os::connect(raw_fd, reinterpret_cast<const sockaddr *>(&addr), addr_len);
    if (UNLIKELY(ret != 0)) {
        ::close(raw_fd);
        throw std::system_error(ret, std::system_category(), "connect");
    }

    return unix_socket{ utils::file_descriptor{ raw_fd } };
}

auto unix_socket::send_msg(struct msghdr &msg) const -> int
{
    return os::sendmsg(fd_.get(), &msg, 0);
}

auto unix_socket::append_fds(struct msghdr &msg, utils::span<const int> fds) -> void
{
    auto *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = static_cast<socklen_t>(CMSG_LEN(fds.size() * sizeof(int)));

    std::memcpy(CMSG_DATA(cmsg), fds.data(), fds.size() * sizeof(int));
}

auto unix_socket::recv_msg(struct msghdr &msg) const -> os::recv_result
{
    return os::recvmsg(fd_.get(), &msg, MSG_CMSG_CLOEXEC);
}

auto unix_socket::extract_fds(struct msghdr &msg) -> std::vector<utils::file_descriptor>
{
    if (CMSG_FIRSTHDR(&msg) == nullptr) {
        return { };
    }

    std::vector<utils::file_descriptor> fds;

    auto *cmsg = CMSG_FIRSTHDR(&msg);
    while (cmsg != nullptr) {
        if (LIKELY(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)) {
            if (UNLIKELY(cmsg->cmsg_len < CMSG_LEN(0))) {
                cmsg = CMSG_NXTHDR(&msg, cmsg);
                continue;
            }

            auto count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (decltype(count) i = 0; i < count; ++i) {
                int raw_fd{ -1 };
                std::memcpy(&raw_fd, CMSG_DATA(cmsg) + (i * sizeof(int)), sizeof(int));
                fds.emplace_back(raw_fd);
            }
        }

        cmsg = CMSG_NXTHDR(&msg, cmsg);
    }

    return fds;
}

auto unix_socket::send(utils::span<const std::byte> data) const -> void
{
    auto r = os::send(fd_.get(), data.data(), data.size(), 0);
    if (UNLIKELY(r != 0)) {
        throw std::system_error(r, std::system_category(), "send");
    }
}

auto unix_socket::send_fd(const utils::file_descriptor &fd) const -> void
{
    LINYAPS_BOX_LOG_DEBUG("Send fd {} to socket {}",
                          utils::inspect_fd(fd.get()),
                          utils::inspect_fd(fd_.get()));
    std::byte placeholder{ };
    send_data_with_fds(utils::span(&placeholder, 1), utils::span(&fd, 1));
}

auto unix_socket::recv_fd() const -> utils::file_descriptor
{
    std::byte placeholder{ };
    auto [fds, bytes] = recv_data_with_fds(utils::span(&placeholder, 1));

    if (UNLIKELY(fds.empty())) {
        throw std::logic_error("recv_fd called but no file descriptor received");
    }

    if (UNLIKELY(fds.size() != 1)) {
        throw std::logic_error(
          fmt::format("recv_fd: expected exactly 1 file descriptor but got {}", fds.size()));
    }

    return std::move(fds.front());
}

auto unix_socket::send_data_with_fds(utils::span<const std::byte> data,
                                     utils::span<const utils::file_descriptor> fds) const -> void
{
    if (UNLIKELY(fds.size() > kMaxScmFds)) {
        throw std::logic_error("too many fds to send at once");
    }

    if (fds.empty()) {
        send(utils::span(data.data(), data.size()));
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Send {} fd(s) with {} data bytes to socket {}",
                          fds.size(),
                          data.size(),
                          utils::inspect_fd(fd_.get()));

    std::vector<int> raw_fds;
    raw_fds.reserve(fds.size());
    std::transform(fds.cbegin(),
                   fds.cend(),
                   std::back_inserter(raw_fds),
                   [](const utils::file_descriptor &fd) {
                       return fd.get();
                   });

    struct iovec iov{ const_cast<std::byte *>(data.data()), data.size() };

    auto cmsg_size = static_cast<socklen_t>(CMSG_SPACE(fds.size() * sizeof(int)));
    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(kMaxScmFds * sizeof(int))> ctrl_buf{ };
    struct msghdr msg{ };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf.data();
    msg.msg_controllen = cmsg_size;

    append_fds(msg, utils::span(raw_fds));
    auto ret = send_msg(msg);
    if (UNLIKELY(ret != 0)) {
        throw std::system_error(ret, std::system_category(), "sendmsg");
    }
}

auto unix_socket::recv_data_with_fds(utils::span<std::byte> data) const
  -> std::pair<std::vector<utils::file_descriptor>, std::size_t>
{
    LINYAPS_BOX_LOG_DEBUG("Receive data with fd from socket {}", utils::inspect_fd(fd_.get()));

    struct iovec iov{ data.data(), data.size() };
    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(kMaxScmFds * sizeof(int))> cmsg_buf{ };
    struct msghdr msg{ };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = static_cast<socklen_t>(cmsg_buf.size());
    auto result = recv_msg(msg);

    if (result.error != 0) {
        throw std::system_error(result.error, std::system_category(), "recvmsg");
    }

    // On SOCK_SEQPACKET, recv_raw already handles peer-close detection via
    // zero-byte MSG_PEEK.  By the time we get here, a non-zero peek was
    // observed, so recvmsg cannot return 0 — the datagram is already queued.
    if (UNLIKELY((msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) != 0)) {
        throw std::runtime_error("message or control data truncated during recvmsg");
    }

    return { extract_fds(msg), result.bytes };
}

auto unix_socket::create_socketpair() -> std::pair<unix_socket, unix_socket>
{
    auto result = os::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (UNLIKELY(!result)) {
        throw std::system_error(result.error, std::system_category(), "socketpair");
    }

    return { unix_socket{ utils::file_descriptor{ result.first } },
             unix_socket{ utils::file_descriptor{ result.second } } };
}

} // namespace linyaps_box::infra
