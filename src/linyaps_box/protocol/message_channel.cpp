// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/message_channel.h"

#include "linyaps_box/os/socket.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/span.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <cerrno>
#include <optional>
#include <stdexcept>
#include <system_error>

#include <unistd.h>

namespace linyaps_box::protocol {

namespace {

auto send_raw(const infra::unix_socket &socket,
              utils::span<const std::byte> buffer,
              utils::span<const utils::file_descriptor> fds) -> void
{
    if (UNLIKELY(fds.size() > linyaps_box::infra::kMaxScmFds)) {
        throw std::logic_error(fmt::format("message_channel: fds count {} exceeds maximum {}, "
                                           "caller should batch-split",
                                           fds.size(),
                                           linyaps_box::infra::kMaxScmFds));
    }

    if (fds.empty()) {
        socket.send(buffer);
        return;
    }

    socket.send_data_with_fds(buffer, fds);
}

struct raw_packet
{
    std::vector<std::byte> data;
    std::vector<utils::file_descriptor> fds;
};

auto recv_raw(const infra::unix_socket &socket) -> std::optional<raw_packet>
{
    std::byte dummy{ };
    auto peek = os::recv(socket.fd().get(), &dummy, 0, MSG_PEEK | MSG_TRUNC);

    // PEER-CLOSE DETECTION ON SEQPACKET
    //
    // On SOCK_SEQPACKET, a zero-byte peek always means the peer has closed its
    // end of the socket.  The protocol never sends 0-byte datagrams, the
    // minimum payload is 1 byte (msg_id).  wait_for_exec() uses this in its
    // second phase: after receiving exec_ready, CLOEXEC socket close signals
    // exec success, while a die message signals exec failure.  Close-before-
    // exec_ready is still treated as an error.
    if (!peek) {
        if (peek.error == ECONNRESET || peek.error == ENOTCONN || peek.error == EPIPE) {
            return std::nullopt;
        }

        throw std::system_error(peek.error, std::system_category(), "recv");
    }

    if (peek.bytes == 0) {
        return std::nullopt;
    }

    std::vector<std::byte> data(static_cast<std::size_t>(peek.bytes));
    auto [fds, n] = socket.recv_data_with_fds(utils::span<std::byte>(data.data(), data.size()));

    if (UNLIKELY(n < sizeof(msg_id))) {
        throw std::runtime_error("truncated message");
    }

    return raw_packet{ std::move(data), std::move(fds) };
}

} // namespace

message_channel_base::message_channel_base(infra::unix_socket socket) noexcept
    : socket_(std::move(socket))
{
}

auto message_channel_base::send(const msg::message &body,
                                utils::span<const utils::file_descriptor> fds) const -> void
{
    auto buffer = msg::serialize(body);
    send_raw(socket_, utils::span<const std::byte>(buffer.data(), buffer.size()), fds);
}

auto message_channel_base::send_stage(stage::type s) const -> void
{
    send(msg::stage{ s });
}

auto message_channel_base::recv() const -> msg::datagram
{
    auto raw = recv_raw(socket_);
    if (UNLIKELY(!raw)) {
        throw std::runtime_error("socket closed by peer");
    }

    auto body = msg::deserialize(utils::span<const std::byte>(raw->data.data(), raw->data.size()));
    return msg::datagram{ std::move(body), std::move(raw->fds) };
}

parent_message_channel::parent_message_channel(infra::unix_socket socket) noexcept
    : message_channel_base(std::move(socket))
{
}

auto parent_message_channel::wait_for(stage::type expected) const -> void
{
    auto inc = recv();

    std::visit(utils::Overload{
                 [&](const msg::die &d) {
                     throw std::system_error(d.errnum, std::system_category(), d.message);
                 },
                 [&](const msg::stage &s) {
                     if (UNLIKELY(s.value != expected)) {
                         throw std::runtime_error(
                           fmt::format("expected stage {} but got {}", expected, s.value));
                     }
                 },
                 [&](const msg::pid_report &) {
                     throw std::runtime_error("unexpected pid_report during wait_for");
                 },
                 [&](const msg::console_fd &) {
                     throw std::runtime_error("unexpected console_fd during wait_for");
                 },
                 [&](const msg::proceed &) {
                     throw std::runtime_error("unexpected proceed during wait_for");
                 },
               },
               inc.body);
}

auto parent_message_channel::wait_for_exec() const -> void
{
    // Phase 1: receive exec_ready or die
    {
        auto raw = recv_raw(socket_);
        if (!raw) {
            throw std::runtime_error("container process closed sync socket before exec_ready");
        }

        auto body =
          msg::deserialize(utils::span<const std::byte>(raw->data.data(), raw->data.size()));
        std::visit(utils::Overload{
                     [&](const msg::die &d) {
                         throw std::system_error(d.errnum, std::system_category(), d.message);
                     },
                     [&](const msg::stage &s) {
                         if (UNLIKELY(s.value != stage::type::exec_ready)) {
                             throw std::runtime_error(
                               fmt::format("expected exec_ready but got {}", s.value));
                         }
                     },
                     [&](const auto &) {
                         throw std::runtime_error("unexpected message during wait_for_exec");
                     },
                   },
                   body);
    }

    // Phase 2: receive either a close (exec success) or die (exec failure)
    auto result = recv_raw(socket_);
    if (!result) {
        return; // success
    }

    auto body =
      msg::deserialize(utils::span<const std::byte>(result->data.data(), result->data.size()));
    std::visit(utils::Overload{
                 [&](const msg::die &d) {
                     throw std::system_error(d.errnum, std::system_category(), d.message);
                 },
                 [&](const auto &) {
                     throw std::runtime_error(
                       "unexpected message after exec_ready (expected close or die)");
                 },
               },
               body);
}

child_message_channel::child_message_channel(infra::unix_socket socket) noexcept
    : message_channel_base(std::move(socket))
{
}

auto child_message_channel::wait_for(stage::type expected) const -> void
{
    auto inc = recv();

    std::visit(
      utils::Overload{
        [&](const msg::die &) {
            throw std::system_error(EPROTO, std::system_category(), "received die from parent");
        },
        [&](const msg::stage &s) {
            if (UNLIKELY(s.value != expected)) {
                throw std::runtime_error(
                  fmt::format("expected stage {} but got {}", expected, s.value));
            }
        },
        [&](const msg::pid_report &) {
            throw std::runtime_error("unexpected pid_report during wait_for");
        },
        [&](const msg::console_fd &) {
            throw std::runtime_error("unexpected console_fd during wait_for");
        },
        [&](const msg::proceed &) {
            throw std::runtime_error("unexpected proceed during wait_for");
        },
      },
      inc.body);
}

auto child_message_channel::wait_for_proceed() const -> void
{
    auto inc = recv();

    std::visit(utils::Overload{
                 [](const msg::proceed &) { },
                 [&](const msg::die &) {
                     throw std::system_error(EPROTO,
                                             std::system_category(),
                                             "received die from parent during wait_for_proceed");
                 },
                 [&](const auto &other) {
                     throw std::runtime_error(
                       fmt::format("unexpected message during wait_for_proceed: {}", other));
                 },
               },
               inc.body);
}

auto child_message_channel::report_error(int errnum, std::string_view text) const noexcept -> void
try {
    send(msg::die{ errnum, std::string{ text } });
} catch (const std::exception &e) {
    LINYAPS_BOX_ERR() << fmt::format("failed to report error to parent (errno={}, msg=\"{}\"): {}",
                                     errnum,
                                     text,
                                     e.what());
} catch (...) {
    LINYAPS_BOX_ERR() << fmt::format(
      "failed to report error to parent (errno={}, msg=\"{}\"): unknown",
      errnum,
      text);
}

auto create_message_socketpair() -> std::pair<parent_message_channel, child_message_channel>
{
    auto [c1, c2] = infra::unix_socket::create_socketpair();
    return { parent_message_channel(std::move(c1)), child_message_channel(std::move(c2)) };
}

} // namespace linyaps_box::protocol
