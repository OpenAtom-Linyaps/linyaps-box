// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"
#include "linyaps_box/protocol/message_channel.h"

#include <functional>

namespace linyaps_box::protocol {
class sync_socket_forwarder final : public log::forwarder
{
public:
    explicit sync_socket_forwarder(child_message_channel &ch) noexcept;
    sync_socket_forwarder(sync_socket_forwarder &&) noexcept = default;
    sync_socket_forwarder &operator=(sync_socket_forwarder &&) noexcept = default;
    sync_socket_forwarder(const sync_socket_forwarder &) = delete;
    sync_socket_forwarder &operator=(const sync_socket_forwarder &) = delete;
    ~sync_socket_forwarder() noexcept = default;

    auto forward(const log::log_context &ctx) noexcept -> void final;

private:
    std::reference_wrapper<child_message_channel> channel;
    std::vector<std::byte> buf;
};
} // namespace linyaps_box::protocol
