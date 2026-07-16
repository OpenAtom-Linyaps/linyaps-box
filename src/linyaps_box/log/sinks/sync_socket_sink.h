// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"
#include "linyaps_box/protocol/message_channel.h"

namespace linyaps_box::log {
class sync_socket_sink
{
public:
    explicit sync_socket_sink(const protocol::child_message_channel &ch) noexcept;
    sync_socket_sink(sync_socket_sink &&) noexcept = default;
    sync_socket_sink &operator=(sync_socket_sink &&) noexcept = default;
    sync_socket_sink(const sync_socket_sink &) = default;
    sync_socket_sink &operator=(const sync_socket_sink &) = default;
    ~sync_socket_sink() noexcept = default;

    auto log(const log_context &ctx) const -> void;

private:
    std::reference_wrapper<const protocol::child_message_channel> channel;
};
} // namespace linyaps_box::log
