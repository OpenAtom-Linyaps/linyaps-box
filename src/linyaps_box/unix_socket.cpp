// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/unix_socket.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"

#include <array>
#include <cstring>

namespace linyaps_box {

unix_socket::unix_socket(linyaps_box::socket socket)
    : socket_(std::move(socket))
{
}

unix_socket unix_socket::connect(const std::filesystem::path &path)
{
    auto addr = socketAddress::unix(path.native());
    auto socket = linyaps_box::socket(socket::Domain::Unix, socket::Type::Seqpacket);
    socket.connect(addr); // Maybe add timeout?
    socket.set_nonblock(true);
    return unix_socket(std::move(socket));
}

// TODO: support request/response
// https://github.com/opencontainers/runtime-tools/blob/v0.9.0/docs/command-line-interface.md#console-socket
auto unix_socket::send_fd(utils::file_descriptor &&fd, std::string_view payload) const -> void
{
    if (!fd.valid()) {
        throw std::runtime_error("invalid file descriptor");
    }

    LINYAPS_BOX_DEBUG() << "Send fd \n"
                        << utils::inspect_fd(fd.get()) << "\n to socket \n"
                        << utils::inspect_fd(socket_.native_handle());

    const auto raw_fd = fd.get();
    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(raw_fd))> ctrl_buf{ };

    std::byte placeholder{ 0 };
    struct iovec iov{ };
    if (payload.empty()) {
        iov.iov_base = &placeholder;
        iov.iov_len = sizeof(placeholder);
    } else {
        iov.iov_base = const_cast<char *>(payload.data());
        iov.iov_len = payload.size();
    }

    struct msghdr msg{ };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl_buf.data();
    msg.msg_controllen = ctrl_buf.size();

    auto *ctrl_msg = CMSG_FIRSTHDR(&msg);
    ctrl_msg->cmsg_level = SOL_SOCKET;
    ctrl_msg->cmsg_type = SCM_RIGHTS;
    ctrl_msg->cmsg_len = CMSG_LEN(sizeof(int));

    std::memcpy(CMSG_DATA(ctrl_msg), &raw_fd, sizeof(raw_fd));
    msg.msg_controllen = ctrl_msg->cmsg_len;

    while (true) {
        auto ret = ::sendmsg(socket_.native_handle(), &msg, 0);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(errno, std::system_category(), "sendmsg");
        }

        break;
    }
}

auto unix_socket::recv_fd(std::string &payload) const -> utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "Receive fd from socket \n"
                        << utils::inspect_fd(socket_.native_handle());

    constexpr auto batch_size = 4096;
    payload.clear();
    payload.reserve(batch_size);

    struct msghdr msg{ };
    struct iovec iov{ payload.data(), batch_size };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(int))> cmsg_buf{ };
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = cmsg_buf.size();

    ssize_t len{ 0 };
    while (true) {
        len = ::recvmsg(socket_.native_handle(), &msg, 0);
        if (len > 0) {
            break;
        }

        if (len == 0) {
            throw std::runtime_error("Socket closed by peer");
        }

        if (errno == EINTR) {
            continue;
        }

        throw std::system_error(errno, std::system_category(), "recvmsg");
    }

    payload.resize(len);

    // TODO: if msg_flags contains MSG_TRUNC， then the message was truncated
    //  we need to read more data from the socket
    //  currently, we just ignore it

    auto *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
        throw std::runtime_error("No file descriptor received in control message");
    }

    int received_fd{ -1 };
    std::memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    if (received_fd < 0) {
        throw std::runtime_error("Invalid file descriptor received in control message");
    }

    LINYAPS_BOX_DEBUG() << "Received fd " << utils::inspect_fd(received_fd);

    return utils::file_descriptor{ received_fd };
}

auto unix_socket::pair() -> std::pair<unix_socket, unix_socket>
{
    std::array<int, 2> fds{ };
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds.data()) == -1) {
        throw std::system_error(errno, std::system_category(), "socketpair");
    }

    return { unix_socket{ fds.at(0) }, unix_socket{ fds.at(1) } };
}

auto unix_socket::operator<<(const std::byte &byte) -> unix_socket &
{
    socket_ << byte;
    return *this;
}

auto unix_socket::operator>>(std::byte &byte) -> unix_socket &
{
    socket_ >> byte;
    return *this;
}

} // namespace linyaps_box
