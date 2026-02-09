// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/unixsocket.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/socket.h"

#include <array>
#include <cstring>

namespace linyaps_box {

unixSocketClient::unixSocketClient(linyaps_box::utils::file_descriptor socket)
    : linyaps_box::utils::file_descriptor(std::move(socket))
{
    if (auto type = this->type(); type != std::filesystem::file_type::socket) {
        auto msg = std::string{ "expected socket, got " };
        msg.append(linyaps_box::utils::to_string(type));
        throw std::runtime_error(msg);
    }
}

unixSocketClient unixSocketClient::connect(const std::filesystem::path &path)
{
    auto fd = linyaps_box::utils::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    fd.set_nonblock(true);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    auto str = path.string();
    if (str.length() >= sizeof(addr.sun_path)) {
        throw std::system_error(ENAMETOOLONG, std::system_category(), "path too long");
    }

    // TODO: add timeout
    std::copy(str.cbegin(), str.cend(), addr.sun_path);
    utils::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));

    return unixSocketClient(std::move(fd));
}

// TODO: support request/response
// https://github.com/opencontainers/runtime-tools/blob/v0.9.0/docs/command-line-interface.md#console-socket
auto unixSocketClient::send_fd(utils::file_descriptor &&fd, std::string_view payload) const -> void
{
    if (!fd.valid()) {
        throw std::runtime_error("invalid file descriptor");
    }

    LINYAPS_BOX_DEBUG() << "Send fd \n"
                        << utils::inspect_fd(fd.get()) << "\n to socket \n"
                        << utils::inspect_fd(this->get());

    const auto raw_fd = fd.get();
    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(raw_fd))> ctrl_buf{};

    std::byte placeholder{ 0 };
    struct iovec iov{};
    if (payload.empty()) {
        iov.iov_base = &placeholder;
        iov.iov_len = sizeof(placeholder);
    } else {
        iov.iov_base = const_cast<char *>(payload.data());
        iov.iov_len = payload.size();
    }

    struct msghdr msg{};
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
        auto ret = ::sendmsg(get(), &msg, 0);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(errno, std::system_category(), "sendmsg");
        }

        break;
    }
}

auto unixSocketClient::recv_fd(std::string &payload) const -> utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "Receive fd from socket \n" << utils::inspect_fd(this->get());

    constexpr auto batch_size = 4096;
    payload.clear();
    payload.reserve(batch_size);

    struct msghdr msg{};
    struct iovec iov{ payload.data(), batch_size };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(int))> cmsg_buf{};
    msg.msg_control = cmsg_buf.data();
    msg.msg_controllen = cmsg_buf.size();

    ssize_t len{ 0 };
    while (true) {
        len = ::recvmsg(get(), &msg, 0);
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

} // namespace linyaps_box
