// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/socket.h>
#include <sys/un.h>

namespace linyaps_box {

class socketAddress
{
public:
    socketAddress(const socketAddress &) = default;
    socketAddress &operator=(const socketAddress &) = default;
    socketAddress(socketAddress &&other) noexcept = default;
    socketAddress &operator=(socketAddress &&other) noexcept = default;
    ~socketAddress() = default;

    static auto unix(std::string_view path) -> socketAddress;

    [[nodiscard]] auto get() const noexcept
    {
        return reinterpret_cast<const sockaddr *>(&storage_);
    }

    [[nodiscard]] auto get() noexcept { return reinterpret_cast<sockaddr *>(&storage_); }

    [[nodiscard]] auto size() const noexcept { return len_; }

    [[nodiscard]] auto size() noexcept -> socklen_t * { return &len_; }

    [[nodiscard]] auto family() const noexcept { return storage_.ss_family; }

    auto clear() noexcept -> void;

private:
    socketAddress() = default;
    sockaddr_storage storage_{ };
    socklen_t len_{ sizeof(storage_) };
};

class socket
{
public:
    enum class Domain : int8_t {
        Unknown = -1,
        Unix = AF_UNIX,
    };

    enum class Type : int8_t {
        Unknown = -1,
        Stream = SOCK_STREAM,
        Dgram = SOCK_DGRAM,
        Seqpacket = SOCK_SEQPACKET
    };

    class passkey
    {
        explicit passkey() = default;
        friend class unix_socket;
    };

    explicit socket(int fd, [[maybe_unused]] passkey passkey)
        : fd_(fd, true)
    {
    }

    socket(Domain domain, Type type, int protocol = 0);

    explicit socket(utils::file_descriptor fd);

    socket(const socket &) = delete;
    socket &operator=(const socket &) = delete;
    socket(socket &&other) noexcept = default;
    socket &operator=(socket &&other) noexcept = default;
    ~socket() = default;

    [[nodiscard]] auto fd() const noexcept -> const utils::file_descriptor & { return fd_; }

    [[nodiscard]] auto native_handle() const & noexcept -> int { return fd_.get(); }

    auto shutdown(int how) const -> void;

    auto connect(const socketAddress &addr) const -> void;

    auto set_nonblock(bool nonblock) & -> void { fd_.set_nonblock(nonblock); }

    template<typename T>
    auto set_option(int level, int optname, const T &value) const -> void
    {
        if (::setsockopt(fd_.get(), level, optname, &value, sizeof(T)) == -1) {
            throw std::system_error(errno, std::system_category(), "setsockopt failed");
        }
    }

    auto bind(const socketAddress &addr) const -> void;

    auto listen(int backlog = SOMAXCONN) const -> void;

    auto accept(socketAddress *peer_addr = nullptr) const -> socket;

    auto release() & -> int { return fd_.release(); }

    auto close() & -> void { fd_.close(); }

    auto operator<<(const std::byte &byte) -> socket &;

    auto operator>>(std::byte &byte) -> socket &;

private:
    utils::file_descriptor fd_;
};

std::string_view to_string(socket::Domain domain) noexcept;

std::string_view to_string(socket::Type type) noexcept;

} // namespace linyaps_box
