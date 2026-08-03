// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/infra/unix_socket.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <array>
#include <stdexcept>

#include <sys/socket.h>

namespace linyaps_box::infra {

unix_socket::unix_socket(utils::file_descriptor fd)
    : fd_(std::move(fd))
{
}

unix_socket unix_socket::connect(const std::filesystem::path &path)
{
    auto ep = os::throw_if_error(os::endpoint::from_path(path));
    auto fd = os::throw_if_error(os::socket(os::sys::address_family::unix,
                                            os::sys::socket_type::seqpacket,
                                            os::sys::socket_flag::cloexec));
    os::throw_if_error(os::connect(fd.ref(), ep));
    return unix_socket{ std::move(fd) };
}

auto unix_socket::send(utils::span<const std::byte> data) const -> os::Result<std::size_t>
{
    return os::send(fd_.ref(), data);
}

auto unix_socket::recv(utils::span<std::byte> buf, os::sys::recv_flag flags) const
  -> os::Result<std::size_t>
{
    return os::recv(fd_.ref(), buf, flags);
}

auto unix_socket::send_fd(utils::file_descriptor_ref fd) const -> os::Result<void>
{
    LINYAPS_BOX_LOG_DEBUG("Send fd {} to socket {}",
                          utils::inspect_fd(fd.get()),
                          utils::inspect_fd(fd_.get()));
    std::byte placeholder{ };
    auto ret = send_data_with_fds(utils::span(&placeholder, 1), utils::span(&fd, 1));
    if (!ret) {
        return os::unexpected(ret.error());
    }

    return { };
}

auto unix_socket::recv_fd() const -> utils::file_descriptor
{
    std::byte placeholder{ };
    auto result = recv_data_with_fds(utils::span(&placeholder, 1));
    if (!result) {
        throw std::system_error(result.error(), "recvmsg");
    }

    auto &fds = result->fds;
    if (UNLIKELY(fds.empty())) {
        throw std::logic_error("recv_fd called but no file descriptor received");
    }

    if (UNLIKELY(fds.size() != 1)) {
        throw std::logic_error(
          fmt::format("recv_fd: expected exactly 1 file descriptor but got {}", fds.size()));
    }

    return std::move(fds.front());
}

// TODO: after removing file_descriptor::auto_close, change this parameter to
// span<const file_descriptor> (see unix_socket.h).
auto unix_socket::send_data_with_fds(utils::span<const std::byte> data,
                                     utils::span<const utils::file_descriptor_ref> fds) const
  -> os::Result<std::size_t>
{
    if (UNLIKELY(fds.size() > kMaxScmFds)) {
        throw std::logic_error("too many fds to send at once");
    }

    if (fds.empty()) {
        return send(data);
    }

    LINYAPS_BOX_LOG_DEBUG("Send {} fd(s) with {} data bytes to socket {}",
                          fds.size(),
                          data.size(),
                          utils::inspect_fd(fd_.get()));

    std::array<std::byte, os::sys::cmsg::buffer_size(os::sys::cmsg::rights{ kMaxScmFds })>
      ctrl_buf{ };
    os::ancillary_buffer_writer writer{ utils::span<std::byte>{ ctrl_buf } };
    if (UNLIKELY(!writer.try_push_back<os::sys::cmsg::rights>(fds))) {
        throw std::logic_error("ancillary buffer too small for fds");
    }

    const os::io_slice iov{ data };
    return os::sendmsg(fd_.ref(), iov, writer);
}

auto unix_socket::recv_data_with_fds(utils::span<std::byte> data) const -> os::Result<recv_result>
{
    LINYAPS_BOX_LOG_DEBUG("Receive data with fd from socket {}", utils::inspect_fd(fd_.get()));

    std::array<std::byte, os::sys::cmsg::buffer_size(os::sys::cmsg::rights{ kMaxScmFds })>
      ctrl_buf{ };

    os::ancillary_buffer control{ utils::span<std::byte>{ ctrl_buf } };

    const os::mutable_io_slice iov{ data };
    auto result = os::recvmsg(fd_.ref(), iov, control, os::sys::recv_flag::cmsg_cloexec);
    if (UNLIKELY(!result)) {
        return os::unexpected(result.error());
    }

    // take the raw fds from the control buffer and wrap them in file_descriptor to take ownership
    auto raw_fds = result->control.rights();
    std::vector<utils::file_descriptor> fds;
    fds.reserve(raw_fds.size());
    for (auto raw_fd : raw_fds) {
        fds.emplace_back(raw_fd);
    }

    if (UNLIKELY((result->flags & (os::sys::return_flag::trunc | os::sys::return_flag::ctrunc))
                 != os::sys::return_flag::none)) {
        throw std::runtime_error("message or control data truncated during recvmsg");
    }

    return recv_result{ std::move(fds), result->bytes };
}

auto unix_socket::create_pair(os::sys::socket_type type, os::sys::socket_flag flag)
  -> std::pair<unix_socket, unix_socket>
{
    auto [a, b] = os::throw_if_error(os::socketpair(os::sys::address_family::unix, type, flag));
    return { unix_socket{ std::move(a) }, unix_socket{ std::move(b) } };
}

} // namespace linyaps_box::infra
