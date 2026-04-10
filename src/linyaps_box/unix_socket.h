// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/socket.h"

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

    auto send_fd(utils::file_descriptor &&fd, std::string_view payload = { }) const -> void;

    [[nodiscard]] auto recv_fd(std::string &payload) const -> utils::file_descriptor;

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

    socket socket_;
};
} // namespace linyaps_box
