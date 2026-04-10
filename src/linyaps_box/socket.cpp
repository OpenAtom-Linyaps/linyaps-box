// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/socket.h"

#include <system_error>

namespace linyaps_box {

auto socketAddress::unix(std::string_view path) -> socketAddress
{
    socketAddress sa;
    auto *un = reinterpret_cast<sockaddr_un *>(&sa.storage_);

    const auto len = std::min(path.size(), sizeof(un->sun_path) - 1);
    if (len != path.size()) {
        throw std::system_error(ENAMETOOLONG, std::system_category(), "socket path too long");
    }

    un->sun_family = AF_UNIX;
    path.copy(un->sun_path, len);
    un->sun_path[len] = '\0';

    sa.len_ = offsetof(struct sockaddr_un, sun_path) + len + 1;
    return sa;
}

auto socketAddress::clear() noexcept -> void
{
    storage_ = { };
    len_ = sizeof(storage_);
}

socket::socket(utils::file_descriptor fd)
    : fd_{ std::move(fd) }
{
    if (!fd_.valid()) {
        throw std::runtime_error("invalid file descriptor");
    }

    if (fd_.type() != std::filesystem::file_type::socket) {
        throw std::runtime_error("not a socket");
    }
}

socket::socket(socket::Domain domain, socket::Type type, int protocol)
{
    if (domain == socket::Domain::Unknown || type == socket::Type::Unknown) {
        throw std::invalid_argument("invalid domain or type");
    }

    auto fd = ::socket(static_cast<int>(domain), static_cast<int>(type) | SOCK_CLOEXEC, protocol);
    if (fd == -1) {
        std::string msg{ "socket(" };
        msg.append(to_string(domain));
        msg.append(", ");
        msg.append(to_string(type));
        msg.append(", ");
        msg.append(std::to_string(protocol));
        msg.append(")");

        throw std::system_error(errno, std::system_category(), msg);
    }

    fd_ = utils::file_descriptor(fd);
}

auto socket::connect(const socketAddress &addr) const -> void
{
    // TODO: add timeout
    if (::connect(fd_.get(), addr.get(), addr.size()) == -1) {
        throw std::system_error(errno, std::system_category(), "connect failed");
    }
}

auto socket::shutdown(int how) const -> void
{
    if (::shutdown(fd_.get(), how) == -1) {
        throw std::system_error(errno, std::system_category(), "shutdown failed");
    }
}

auto socket::bind(const socketAddress &addr) const -> void
{
    if (::bind(fd_.get(), addr.get(), addr.size()) == -1) {
        throw std::system_error(errno, std::system_category(), "bind failed");
    }
}

auto socket::listen(int backlog) const -> void
{
    if (::listen(fd_.get(), backlog) == -1) {
        throw std::system_error(errno, std::system_category(), "listen failed");
    }
}

auto socket::accept(socketAddress *peer_addr) const -> socket
{
    socklen_t *plen = peer_addr == nullptr ? peer_addr->size() : nullptr;
    sockaddr *paddr = peer_addr == nullptr ? peer_addr->get() : nullptr;

    auto client_fd = ::accept4(fd_.get(), paddr, plen, SOCK_CLOEXEC);
    if (client_fd == -1) {
        throw std::system_error(errno, std::system_category(), "accept failed");
    }

    return socket(utils::file_descriptor(client_fd, true));
}

auto socket::operator<<(const std::byte &byte) -> socket &
{
    fd_ << byte;
    return *this;
}

auto socket::operator>>(std::byte &byte) -> socket &
{
    fd_ >> byte;
    return *this;
}

std::string_view to_string(socket::Domain domain) noexcept
{
    switch (domain) {
    case socket::Domain::Unknown:
        return "Unknown";
    case socket::Domain::Unix:
        return "Unix";
    default:
        __builtin_unreachable();
    }
}

std::string_view to_string(socket::Type type) noexcept
{
    switch (type) {
    case socket::Type::Unknown:
        return "Unknown";
    case socket::Type::Stream:
        return "Stream";
    case socket::Type::Dgram:
        return "Dgram";
    case socket::Type::Seqpacket:
        return "Seqpacket";
    default:
        __builtin_unreachable();
    }
}

} // namespace linyaps_box
