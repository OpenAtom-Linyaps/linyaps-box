// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/infra/unix_socket.h"
#include "linyaps_box/protocol/message.h"

#include <optional>
#include <utility>

namespace linyaps_box::protocol {

class sync_socket_forwarder;
class child_message_channel;

class channel_transport
{
public:
    explicit channel_transport(infra::unix_socket socket) noexcept;

    channel_transport(const channel_transport &) = delete;
    auto operator=(const channel_transport &) -> channel_transport & = delete;
    channel_transport(channel_transport &&) noexcept = default;
    auto operator=(channel_transport &&) noexcept -> channel_transport & = default;
    ~channel_transport() noexcept = default;

    auto send(const msg::message &body, utils::span<const utils::file_descriptor_ref> fds = { })
      -> void;

    [[nodiscard]] auto recv() -> std::optional<msg::datagram>;

    auto close() & -> void { socket.close(); }

private:
    friend class child_message_channel;

    infra::unix_socket socket;
    std::vector<std::byte> data;

    // Bypass for callers that already hold pre-serialized wire bytes
    // (e.g. sync_socket_forwarder).
    // Most callers should use the public send()
    // which serializes msg::message internally.
    auto send_bytes(utils::span<const std::byte> buffer,
                    utils::span<const utils::file_descriptor_ref> fds) -> void;
};

class parent_message_channel
{
    channel_transport transport;

    friend auto create_message_socketpair()
      -> std::pair<parent_message_channel, child_message_channel>;
    explicit parent_message_channel(infra::unix_socket socket) noexcept;

public:
    parent_message_channel(const parent_message_channel &) = delete;
    auto operator=(const parent_message_channel &) -> parent_message_channel & = delete;
    parent_message_channel(parent_message_channel &&) noexcept = default;
    auto operator=(parent_message_channel &&) noexcept -> parent_message_channel & = default;
    ~parent_message_channel() noexcept = default;

    auto send_stage(stage::type s) -> void;
    auto send_proceed() -> void;
    auto wait_for_stage(stage::type expected) -> void;
    auto wait_for_close() -> void;
    [[nodiscard]] auto drain_logs() -> msg::datagram;

    auto close() & -> void { transport.close(); }
};

class child_message_channel
{
    channel_transport transport;

    friend auto create_message_socketpair()
      -> std::pair<parent_message_channel, child_message_channel>;
    friend class sync_socket_forwarder;
    explicit child_message_channel(infra::unix_socket socket) noexcept;

    // Private: only sync_socket_forwarder may inject pre-serialized wire bytes
    // (it owns the log_context → wire serialization on the child side).
    //
    // Why friend + asymmetric API: the parent/child channels are NOT peers.
    // Logs flow strictly child→parent — the child forwards via the forwarder,
    // the parent drains via wait_for_stage()/drain_logs()/wait_for_close().
    // The child side only sends control messages (stage/pid_report/console_fd)
    // and receives control messages (expect_stage/expect_proceed); it never
    // drains logs.  Exposing a general raw-bytes sender to all callers would
    // let arbitrary code forge wire frames, so the single legitimate user is
    // friended instead.
    auto send_bytes(utils::span<const std::byte> buffer) -> void;

public:
    child_message_channel(const child_message_channel &) = delete;
    auto operator=(const child_message_channel &) -> child_message_channel & = delete;
    child_message_channel(child_message_channel &&) noexcept = default;
    auto operator=(child_message_channel &&) noexcept -> child_message_channel & = default;
    ~child_message_channel() noexcept = default;

    auto send_stage(stage::type s) -> void;
    auto send_pid_report(pid_t pid) -> void;
    auto send_console_fd(utils::file_descriptor_ref fd) -> void;
    auto expect_stage(stage::type expected) -> void;
    auto expect_proceed() -> void;

    auto close() & -> void { transport.close(); }
};

auto create_message_socketpair() -> std::pair<parent_message_channel, child_message_channel>;

} // namespace linyaps_box::protocol
