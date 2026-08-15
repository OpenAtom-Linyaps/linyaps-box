// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/protocol/sync_socket_forwarder.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/protocol/message.h"
#include "linyaps_box/utils/span.h"

#include <unistd.h>

namespace linyaps_box::protocol {

sync_socket_forwarder::sync_socket_forwarder(child_message_channel &ch) noexcept
    : channel(ch)
{
}

auto sync_socket_forwarder::forward(const log::log_context &ctx) noexcept -> void
try {
    buf.clear();
    msg::serialize_log_into(buf, ctx);
    channel.get().send_bytes(utils::span(buf));
} catch (...) {
    fmt::memory_buffer fallback_buf;
    log::format_log(fallback_buf, ctx, log::output_format::text, { });

    const utils::file_descriptor stderr_fd{ STDERR_FILENO, false };
    auto bytes = utils::as_bytes(utils::span(fallback_buf.data(), fallback_buf.size()));
    std::ignore = stderr_fd.write_span(bytes);
}

} // namespace linyaps_box::protocol
