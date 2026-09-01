// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/net.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <vector>

#include <sys/un.h>

namespace linyaps_box::os {

namespace sys::cmsg {

std::optional<credentials::value_type> credentials::parse(utils::span<const std::byte> raw) noexcept
{
    constexpr auto data_size = sizeof(value_type);
    if (raw.size() < data_size) {
        return std::nullopt;
    }

    value_type cred{ };
    std::memcpy(&cred, raw.data(), data_size);
    return cred;
}

} // namespace sys::cmsg

endpoint::endpoint(const sockaddr *addr, socklen_t len) noexcept
{
    if (addr != nullptr && len >= sizeof(sa_family_t) && len <= sizeof(storage_)) {
        std::memcpy(&storage_, addr, len);
        len_ = len;
    }
}

auto endpoint::from_path(const std::filesystem::path &path) noexcept -> os::Result<endpoint>
{
    endpoint ep;
    const std::string_view str{ path.native() };
    auto *un = reinterpret_cast<sockaddr_un *>(&ep.storage_);
    if (str.size() >= sizeof(un->sun_path)) {
        return unexpected{ std::make_error_code(std::errc::filename_too_long) };
    }

    un->sun_family = AF_UNIX;

    std::copy(str.cbegin(), str.cend(), un->sun_path);
    un->sun_path[str.size()] = '\0';
    ep.len_ = static_cast<socklen_t>(offsetof(::sockaddr_un, sun_path) + str.size() + 1);
    return ep;
}

auto send(utils::file_descriptor_ref fd,
          utils::span<const std::byte> buf,
          utils::bitflags<sys::send_flag> flags) noexcept -> Result<std::size_t>
{
    while (true) {
        auto ret = ::send(fd, buf.data(), buf.size(), flags.to_raw());
        if (UNLIKELY(ret < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return ret;
    }
}

auto sendmsg(utils::file_descriptor_ref fd,
             const io_slice &iov,
             ancillary_buffer_writer &control,
             utils::bitflags<sys::send_flag> flags) noexcept -> Result<std::size_t>
{
    struct msghdr msg{ };

    msg.msg_iov = const_cast<struct iovec *>(iov.iov());
    msg.msg_iovlen = 1;

    const auto control_span = control.as_span();
    if (!control_span.empty()) {
        msg.msg_control = const_cast<void *>(static_cast<const void *>(control_span.data()));
        msg.msg_controllen = control_span.size_bytes();
    }

    while (true) {
        auto ret = ::sendmsg(fd, &msg, flags.to_raw());
        if (UNLIKELY(ret < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return static_cast<std::size_t>(ret);
    }
}

auto recv(utils::file_descriptor_ref fd,
          utils::span<std::byte> buf,
          utils::bitflags<sys::recv_flag> flags) noexcept -> Result<std::size_t>
{
    while (true) {
        auto ret = ::recv(fd, buf.data(), buf.size(), flags.to_raw());
        if (UNLIKELY(ret < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return ret;
    }
}

auto recvmsg(utils::file_descriptor_ref fd,
             const mutable_io_slice &iov,
             ancillary_buffer &control,
             utils::bitflags<sys::recv_flag> flags) noexcept -> Result<RecvMsg>
{
    struct msghdr msg{ };

    msg.msg_iov = const_cast<struct iovec *>(iov.iov());
    msg.msg_iovlen = 1;

    msg.msg_control = control.data();
    msg.msg_controllen = control.size();

    endpoint peer_ep;
    msg.msg_name = peer_ep.data_mut();
    msg.msg_namelen = linyaps_box::os::endpoint::capacity();

    while (true) {
        auto ret = ::recvmsg(fd.get(), &msg, flags.to_raw());
        if (UNLIKELY(ret < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        RecvMsg res{ };
        res.bytes = static_cast<std::size_t>(ret);
        res.flags = sys::return_flag(msg.msg_flags);

        if (msg.msg_namelen > 0) {
            res.address.emplace(peer_ep.data(), msg.msg_namelen);
        }

        if (msg.msg_control != nullptr && msg.msg_controllen > 0) {
            res.control = ancillary_buffer_view{ utils::span<const std::byte>{
              reinterpret_cast<const std::byte *>(msg.msg_control),
              msg.msg_controllen } };
        }

        return res;
    }
}

auto ancillary_buffer_view::rights() const noexcept -> std::vector<int>
{
    std::vector<int> fds;
    for (const auto &cmsg : *this) {
        if (cmsg.template is<sys::cmsg::rights>()) {
            const auto fd_span = cmsg.template get<sys::cmsg::rights>();
            fds.insert(fds.end(), fd_span.begin(), fd_span.end());
        }
    }

    return fds;
}

auto ancillary_buffer_writer::try_push_raw(int level,
                                           int type,
                                           const void *data,
                                           size_type len) noexcept -> bool
{
    const size_type space_needed = CMSG_SPACE(len);
    if (size_ + space_needed > buffer_.size()) {
        return false;
    }

    auto *cmsg = reinterpret_cast<struct cmsghdr *>(buffer_.data() + size_);
    cmsg->cmsg_level = level;
    cmsg->cmsg_type = type;
    cmsg->cmsg_len = CMSG_LEN(len);

    if (len > 0 && data != nullptr) {
        std::copy_n(static_cast<const std::byte *>(data),
                    len,
                    reinterpret_cast<std::byte *>(CMSG_DATA(cmsg)));
    }

    size_ += space_needed;
    return true;
}

auto ancillary_message_view::raw_data() const noexcept -> utils::span<const std::byte>
{
    if (cmsg_ == nullptr) {
        return { };
    }

    const auto data_len = cmsg_->cmsg_len >= CMSG_LEN(0) ? cmsg_->cmsg_len - CMSG_LEN(0) : 0;
    if (data_len == 0) {
        return { };
    }

    return { reinterpret_cast<const std::byte *>(CMSG_DATA(cmsg_)), data_len };
}

auto socket(sys::address_family domain,
            sys::socket_type type,
            utils::bitflags<sys::socket_flag> flag,
            int protocol) noexcept -> Result<utils::file_descriptor>
{
    auto fd = ::socket(static_cast<int>(domain), static_cast<int>(type) | flag.to_raw(), protocol);
    if (UNLIKELY(fd == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return utils::file_descriptor{ fd };
}

auto socketpair(sys::address_family domain,
                sys::socket_type type,
                utils::bitflags<sys::socket_flag> flag,
                int protocol) noexcept
  -> Result<std::pair<utils::file_descriptor, utils::file_descriptor>>
{
    std::array<int, 2> fds{ };
    if (UNLIKELY(::socketpair(static_cast<int>(domain),
                              static_cast<int>(type) | flag.to_raw(),
                              protocol,
                              fds.data())
                 == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return std::make_pair(utils::file_descriptor{ fds[0] }, utils::file_descriptor{ fds[1] });
}

auto connect(utils::file_descriptor_ref fd, const endpoint &ep) noexcept -> Result<void>
{
    while (true) {
        if (LIKELY(::connect(fd, ep.data(), ep.size()) == 0)) {
            return { };
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}

} // namespace linyaps_box::os
