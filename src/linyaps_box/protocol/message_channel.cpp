// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/message_channel.h"

#include "linyaps_box/log/logger.h"
#include "linyaps_box/utils/span.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <cassert>
#include <cerrno>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <system_error>

#include <unistd.h>

namespace linyaps_box::protocol {

channel_transport::channel_transport(infra::unix_socket socket) noexcept
    : socket(std::move(socket))
{
}

auto channel_transport::send(const msg::message &body,
                             utils::span<const utils::file_descriptor_ref> fds) -> void
{
    const auto buffer = msg::serialize(body);
    send_bytes(buffer, fds);
}

auto channel_transport::send_bytes(utils::span<const std::byte> buffer,
                                   utils::span<const utils::file_descriptor_ref> fds) -> void
{
    if (UNLIKELY(fds.size() > linyaps_box::infra::kMaxScmFds)) {
        throw std::logic_error(fmt::format("message_channel: fds count {} exceeds maximum {}, "
                                           "caller should batch-split",
                                           fds.size(),
                                           linyaps_box::infra::kMaxScmFds));
    }

    // message_channel sockets are always SOCK_SEQPACKET, which sends each
    // datagram atomically; a short write would silently truncate the message
    // and can only mean the socket was misused (e.g. a stream socket).
    auto sent = fds.empty() ? socket.send(buffer) : socket.send_data_with_fds(buffer, fds);
    if (UNLIKELY(!sent)) {
        throw std::system_error(std::move(sent).error(), "failed to send message_channel datagram");
    }

    if (UNLIKELY(*sent != buffer.size())) {
        throw std::system_error(
          std::make_error_code(std::errc::io_error),
          fmt::format("short write on message_channel socket, sent {} of {} bytes",
                      *sent,
                      buffer.size()));
    }
}

auto channel_transport::recv() -> std::optional<msg::datagram>
{
    std::byte dummy{ };
    auto peek = socket.recv(utils::span(&dummy, static_cast<std::size_t>(0)),
                            os::sys::recv_flag::peek | os::sys::recv_flag::trunc);
    if (!peek) {
        const auto err = peek.error().value();
        if (err == ECONNRESET || err == ENOTCONN || err == EPIPE) {
            return std::nullopt;
        }

        throw std::system_error(std::move(peek).error(), "failed to peek message_channel datagram");
    }

    // PEER-CLOSE DETECTION ON SEQPACKET
    //
    // On SOCK_SEQPACKET, a zero-byte peek always means the peer has closed its
    // end of the socket.  The protocol never sends 0-byte datagrams, the
    // minimum payload is 1 byte (msg_id).  wait_for_exec() uses this in its
    // second phase: after receiving exec_ready, CLOEXEC socket close signals
    // exec success, while a die message signals exec failure.  Close-before-
    // exec_ready is still treated as an error.
    if (*peek == 0) {
        return std::nullopt;
    }

    data.resize(*peek);
    auto result = socket.recv_data_with_fds(data);
    if (!result) {
        const auto err = result.error().value();
        if (err == ECONNRESET || err == ENOTCONN || err == EPIPE) {
            return std::nullopt;
        }

        throw std::system_error(std::move(result).error(), "recvmsg");
    }

    auto [fds, n] = std::move(*result);
    if (UNLIKELY(n < sizeof(msg_id))) {
        throw std::runtime_error("truncated message");
    }

    auto body = msg::deserialize(utils::span(data.data(), n));
    return msg::datagram{ std::move(body), std::move(fds) };
}

parent_message_channel::parent_message_channel(infra::unix_socket socket) noexcept
    : transport(std::move(socket))
{
}

auto parent_message_channel::send_stage(stage::type s) -> void
{
    transport.send(msg::stage{ s });
}

auto parent_message_channel::send_proceed() -> void
{
    transport.send(msg::proceed{ });
}

auto parent_message_channel::wait_for_stage(stage::type expected) -> void
{
    while (true) {
        auto inc = transport.recv();
        if (UNLIKELY(!inc)) {
            throw std::runtime_error(
              fmt::format("container process exited before reaching expected stage {}", expected));
        }

        const auto done =
          std::visit(utils::Overload{
                       [&](msg::log &l) {
                           assert(inc->fds.empty());
                           log::global_logger::instance().dispatch_context(msg::to_log_context(l));
                           return false;
                       },
                       [&](const msg::stage &s) {
                           if (UNLIKELY(s.value != expected)) {
                               throw std::runtime_error(
                                 fmt::format("expected stage {} but got {}", expected, s.value));
                           }

                           return true;
                       },
                       [&](const auto &) -> bool {
                           throw std::runtime_error("unexpected message during wait_for_stage");
                       },
                     },
                     inc->body);

        if (done) {
            return;
        }
    }
}

auto parent_message_channel::wait_for_close() -> void
{
    // Contract: a socket close without any preceding FATAL log is treated as
    // "exec succeeded".
    // The child's exit code is not known here — it is
    // reaped later by container_monitor::wait_container_exit().
    // Callers that need to distinguish "exec ok" from "child died silently (e.g. OOM/signal
    // before it could log FATAL)" must additionally consult the monitor; this
    // function cannot tell those apart.
    bool saw_log{ false };
    int saved_errno{ 0 };
    while (true) {
        auto inc = transport.recv();
        if (!inc) {
            if (UNLIKELY(saw_log)) {
                throw std::system_error(saved_errno,
                                        std::system_category(),
                                        "container process failed during exec");
            }

            return;
        }

        std::visit(utils::Overload{
                     [&](msg::log &l) {
                         assert(inc->fds.empty());
                         if (l.lvl == linyaps_box::log::level::fatal) {
                             saw_log = true;
                             saved_errno = l.errno_;
                         }

                         log::global_logger::instance().dispatch_context(msg::to_log_context(l));
                     },
                     [&](const auto &) {
                         throw std::runtime_error("unexpected message after exec_ready");
                     },
                   },
                   inc->body);
    }
}

auto parent_message_channel::drain_logs() -> msg::datagram
{
    while (true) {
        auto inc = transport.recv();
        if (UNLIKELY(!inc)) {
            throw std::runtime_error("child process exited before sending expected message");
        }

        if (!std::holds_alternative<msg::log>(inc->body)) {
            return std::move(inc).value();
        }

        assert(inc->fds.empty());
        const auto &l = std::get<msg::log>(inc->body);
        log::global_logger::instance().dispatch_context(msg::to_log_context(l));
    }
}

child_message_channel::child_message_channel(infra::unix_socket socket) noexcept
    : transport(std::move(socket))
{
}

auto child_message_channel::send_stage(stage::type s) -> void
{
    transport.send(msg::stage{ s });
}

auto child_message_channel::send_pid_report(pid_t pid) -> void
{
    transport.send(msg::pid_report{ pid });
}

auto child_message_channel::send_console_fd(utils::file_descriptor_ref fd) -> void
{
    transport.send(msg::console_fd{ }, utils::span<const utils::file_descriptor_ref>{ &fd, 1 });
}

auto child_message_channel::send_bytes(utils::span<const std::byte> buffer) -> void
{
    transport.send_bytes(buffer, { });
}

auto child_message_channel::expect_stage(stage::type expected) -> void
{
    auto inc = transport.recv();
    if (UNLIKELY(!inc)) {
        throw std::runtime_error(
          fmt::format("container process exited before reaching expected stage {}", expected));
    }

    std::visit(utils::Overload{
                 [&](const msg::stage &s) {
                     if (UNLIKELY(s.value != expected)) {
                         throw std::runtime_error(
                           fmt::format("expected stage {} but got {}", expected, s.value));
                     }
                 },
                 [&](const auto &) {
                     throw std::runtime_error("unexpected message during expect_stage");
                 },
               },
               inc->body);
}

auto child_message_channel::expect_proceed() -> void
{
    auto inc = transport.recv();
    if (UNLIKELY(!inc)) {
        throw std::runtime_error("socket closed before receiving proceed");
    }

    std::visit(utils::Overload{
                 [](const msg::proceed &) { },
                 [&](const auto &other) {
                     throw std::runtime_error(
                       fmt::format("unexpected message during expect_proceed: {}", other));
                 },
               },
               inc->body);
}

auto create_message_socketpair() -> std::pair<parent_message_channel, child_message_channel>
{
    auto [c1, c2] = infra::unix_socket::create_pair(os::sys::socket_type::seqpacket,
                                                    os::sys::socket_flag::cloexec);
    return { parent_message_channel(std::move(c1)), child_message_channel(std::move(c2)) };
}

} // namespace linyaps_box::protocol
