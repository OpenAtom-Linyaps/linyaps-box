// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/socket.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/utils.h"

namespace linyaps_box {
class unix_socket
{
public:
    explicit unix_socket(linyaps_box::socket socket);

    static unix_socket connect(const std::filesystem::path &path);

    unix_socket(const unix_socket &) = delete;
    auto operator=(const unix_socket &) -> unix_socket & = delete;

    unix_socket(unix_socket &&other) noexcept = default;
    auto operator=(unix_socket &&other) noexcept -> unix_socket & = default;

    ~unix_socket() = default;

    auto send_fd(utils::file_descriptor fd, std::string_view payload = { }) const -> void;

    [[nodiscard]] auto recv_fd(std::string &payload) const -> utils::file_descriptor;

    template <typename T>
    auto send_msg(const T &msg) const -> void
    {
        static_assert(std::is_trivially_copyable_v<T>);
        auto result = socket_.fd().write(msg);
        if (UNLIKELY(result.status != utils::IOStatus::Success)) {
            throw std::runtime_error("send_msg failed");
        }
    }

    template <typename T>
    [[nodiscard]] auto recv_msg() const -> T
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T msg{ };
        auto result = socket_.fd().read(msg);
        if (UNLIKELY(result.status != utils::IOStatus::Success)) {
            throw std::runtime_error("recv_msg failed");
        }

        return msg;
    }

    template <typename T>
    auto send_msg_with_fd(utils::file_descriptor fd, const T &msg) const -> void
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (!fd.valid()) {
            throw std::runtime_error("invalid file descriptor for send_msg_with_fd");
        }

        LINYAPS_BOX_DEBUG() << "Send fd " << utils::inspect_fd(fd.get());
        struct iovec iov{ };
        iov.iov_base = const_cast<T *>(&msg);
        iov.iov_len = sizeof(msg);
        send_cmsg(iov, fd.get());
    }

    template <typename T>
    [[nodiscard]] auto recv_msg_with_fd() const -> std::pair<T, utils::file_descriptor>
    {
        static_assert(std::is_trivially_copyable_v<T>);
        LINYAPS_BOX_DEBUG() << "Receive fd from socket "
                            << utils::inspect_fd(socket_.native_handle());
        T msg{ };
        struct iovec iov{ };
        iov.iov_base = &msg;
        iov.iov_len = sizeof(msg);
        size_t data_len{ 0 };
        auto received_fd = recv_cmsg(iov, data_len);
        LINYAPS_BOX_DEBUG() << "Received fd " << utils::inspect_fd(received_fd);
        return { msg, utils::file_descriptor{ received_fd } };
    }

    [[nodiscard]] auto fd() const & noexcept -> const utils::file_descriptor &
    {
        return socket_.fd();
    }

    auto release() & -> int { return socket_.release(); }

    auto close() & -> void { socket_.close(); }

    static auto pair() -> std::pair<unix_socket, unix_socket>;

    auto operator<<(const std::byte &byte) -> unix_socket &;

    auto operator>>(std::byte &byte) -> unix_socket &;

private:
    explicit unix_socket(int fd)
        : socket_(fd, socket::passkey{ })
    {
    }

    auto send_cmsg(struct iovec &iov, int fd) const -> void;
    [[nodiscard]] auto recv_cmsg(struct iovec &iov, size_t &data_len) const -> int;

    socket socket_;
};
} // namespace linyaps_box
