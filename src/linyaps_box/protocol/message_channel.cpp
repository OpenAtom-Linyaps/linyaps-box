// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/message_channel.h"

#include "linyaps_box/log/logger.h"
#include "linyaps_box/os/socket.h"
#include "linyaps_box/utils/span.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <cassert>
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

auto recv_raw(const infra::unix_socket &socket) -> std::optional<raw_packet>;

// Common helper: recv one message, deserialize, return nullopt on peer close.
[[nodiscard]] auto recv_one(const infra::unix_socket &socket) -> std::optional<msg::datagram>
{
    auto raw = recv_raw(socket);
    if (!raw) {
        return std::nullopt;
    }

    auto body = msg::deserialize(utils::span<const std::byte>(raw->data.data(), raw->data.size()));
    return msg::datagram{ std::move(body), std::move(raw->fds) };
}

auto recv_raw(const infra::unix_socket &socket) -> std::optional<raw_packet>
{
    std::byte dummy{ };
    auto peek = os::recv(socket.fd().get(), &dummy, 0, MSG_PEEK | MSG_TRUNC);
    if (!peek) {
        if (peek.error == ECONNRESET || peek.error == ENOTCONN || peek.error == EPIPE) {
            return std::nullopt;
        }

        throw std::system_error(peek.error, std::system_category(), "recv");
    }

    // PEER-CLOSE DETECTION ON SEQPACKET
    //
    // On SOCK_SEQPACKET, a zero-byte peek always means the peer has closed its
    // end of the socket.  The protocol never sends 0-byte datagrams, the
    // minimum payload is 1 byte (msg_id).  wait_for_exec() uses this in its
    // second phase: after receiving exec_ready, CLOEXEC socket close signals
    // exec success, while a die message signals exec failure.  Close-before-
    // exec_ready is still treated as an error.
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

auto message_channel_base::send_log(const linyaps_box::log::log_context &ctx) const -> void
{
    const msg::log m{ std::string{ ctx.msg },
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
                      std::string{ ctx.file },
                      std::string{ ctx.function },
                      ctx.line,
#endif
                      ctx.errno_,
                      std::chrono::duration_cast<std::chrono::nanoseconds>(
                        ctx.wall_time.time_since_epoch()),
                      ctx.pid,
                      ctx.lvl };

    auto buffer = msg::serialize_log(m);
    send_raw(socket_, utils::span(buffer), { });
}

auto message_channel_base::recv() const -> msg::datagram
{
    auto inc = recv_one(socket_);
    if (UNLIKELY(!inc)) {
        throw std::runtime_error("socket closed by peer");
    }

    return std::move(inc).value();
}

void forward_log_to_parent(msg::log l)
{
    auto tp = std::chrono::system_clock::time_point{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(l.time)
    };

    const linyaps_box::log::log_context ctx{
        l.lvl,  l.message,  tp,     l.pid, l.errno_,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        l.file, l.function, l.line,
#endif
    };
    linyaps_box::log::global_logger::instance().dispatch_raw(ctx);
}

parent_message_channel::parent_message_channel(infra::unix_socket socket) noexcept
    : message_channel_base(std::move(socket))
{
}

auto parent_message_channel::wait_for(stage::type expected) const -> void
{
    while (true) {
        auto inc = recv_one(socket_);
        if (UNLIKELY(!inc)) {
            throw std::runtime_error(
              fmt::format("container process exited before reaching expected stage {}", expected));
        }

        bool matched{ false };
        std::visit(utils::Overload{
                     [&](msg::log &l) {
                         assert(inc->fds.empty());
                         forward_log_to_parent(std::move(l));
                     },
                     [&](const msg::stage &s) {
                         if (UNLIKELY(s.value != expected)) {
                             throw std::runtime_error(
                               fmt::format("expected stage {} but got {}", expected, s.value));
                         }

                         matched = true;
                     },
                     [&](const auto &) {
                         throw std::runtime_error("unexpected message during wait_for");
                     },
                   },
                   inc->body);

        if (matched) {
            return;
        }
    }
}

auto parent_message_channel::wait_for_close() const -> void
{
    // Contract: a socket close without any preceding FATAL log is treated as
    // "exec succeeded".  The child's exit code is not known here — it is
    // reaped later by container_monitor::wait_container_exit().  Callers that
    // need to distinguish "exec ok" from "child died silently (e.g. OOM/signal
    // before it could log FATAL)" must additionally consult the monitor; this
    // function cannot tell those apart.
    bool saw_log{ false };
    int saved_errno{ 0 };
    while (true) {
        auto inc = recv_one(socket_);
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

                         forward_log_to_parent(std::move(l));
                     },
                     [&](const auto &) {
                         throw std::runtime_error("unexpected message after exec_ready");
                     },
                   },
                   inc->body);
    }
}

auto parent_message_channel::drain_logs() const -> msg::datagram
{
    while (true) {
        auto inc = recv_one(socket_);
        if (UNLIKELY(!inc)) {
            throw std::runtime_error("child process exited before sending expected message");
        }

        if (!std::holds_alternative<msg::log>(inc->body)) {
            return std::move(inc).value();
        }

        assert(inc->fds.empty());
        forward_log_to_parent(std::move(std::get<msg::log>(inc->body)));
    }
}

child_message_channel::child_message_channel(infra::unix_socket socket) noexcept
    : message_channel_base(std::move(socket))
{
}

auto child_message_channel::wait_for(stage::type expected) const -> void
{
    auto inc = recv();

    std::visit(utils::Overload{
                 [&](const msg::stage &s) {
                     if (UNLIKELY(s.value != expected)) {
                         throw std::runtime_error(
                           fmt::format("expected stage {} but got {}", expected, s.value));
                     }
                 },
                 [&](const auto &) {
                     throw std::runtime_error("unexpected message during child wait_for");
                 },
               },
               inc.body);
}

auto child_message_channel::wait_for_proceed() const -> void
{
    auto inc = recv();

    std::visit(utils::Overload{
                 [](const msg::proceed &) { },
                 [&](const auto &other) {
                     throw std::runtime_error(
                       fmt::format("unexpected message during wait_for_proceed: {}", other));
                 },
               },
               inc.body);
}

auto create_message_socketpair() -> std::pair<parent_message_channel, child_message_channel>
{
    auto [c1, c2] = infra::unix_socket::create_socketpair();
    return { parent_message_channel(std::move(c1)), child_message_channel(std::move(c2)) };
}

} // namespace linyaps_box::protocol
