// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/infra/unix_socket.h"
#include "linyaps_box/protocol/message.h"

#include <utility>

namespace linyaps_box::protocol {

class child_message_channel;

class message_channel_base
{
public:
    explicit message_channel_base(infra::unix_socket socket) noexcept;

    message_channel_base(const message_channel_base &) = delete;
    auto operator=(const message_channel_base &) -> message_channel_base & = delete;
    message_channel_base(message_channel_base &&) noexcept = default;
    auto operator=(message_channel_base &&) noexcept -> message_channel_base & = default;
    ~message_channel_base() noexcept = default;

    auto send(const msg::message &body,
              utils::span<const utils::file_descriptor_ref> fds = { }) const -> void;

    auto send_log(const linyaps_box::log::log_context &ctx) const -> void;

    auto send_stage(stage::type s) const -> void;

    [[nodiscard]] auto recv() const -> msg::datagram;

    auto close() & -> void { socket_.close(); }

protected:
    infra::unix_socket socket_;
};

class parent_message_channel final : public message_channel_base
{
public:
    parent_message_channel(const parent_message_channel &) = delete;
    auto operator=(const parent_message_channel &) -> parent_message_channel & = delete;
    parent_message_channel(parent_message_channel &&) noexcept = default;
    auto operator=(parent_message_channel &&) noexcept -> parent_message_channel & = default;
    ~parent_message_channel() noexcept = default;

    auto wait_for(stage::type expected) const -> void;
    auto wait_for_close() const -> void;
    [[nodiscard]] auto drain_logs() const -> msg::datagram;

private:
    friend auto create_message_socketpair()
      -> std::pair<parent_message_channel, child_message_channel>;
    explicit parent_message_channel(infra::unix_socket socket) noexcept;
};

class child_message_channel final : public message_channel_base
{
public:
    child_message_channel(const child_message_channel &) = delete;
    auto operator=(const child_message_channel &) -> child_message_channel & = delete;
    child_message_channel(child_message_channel &&) noexcept = default;
    auto operator=(child_message_channel &&) noexcept -> child_message_channel & = default;
    ~child_message_channel() noexcept = default;

    auto wait_for(stage::type expected) const -> void;
    auto wait_for_proceed() const -> void;

private:
    friend auto create_message_socketpair()
      -> std::pair<parent_message_channel, child_message_channel>;
    explicit child_message_channel(infra::unix_socket socket) noexcept;
};

auto create_message_socketpair() -> std::pair<parent_message_channel, child_message_channel>;

} // namespace linyaps_box::protocol
