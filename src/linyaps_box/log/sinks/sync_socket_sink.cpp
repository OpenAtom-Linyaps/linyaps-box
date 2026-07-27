// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/sync_socket_sink.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/utils/span.h"

#include <unistd.h>

namespace linyaps_box::log {

sync_socket_sink::sync_socket_sink(const protocol::child_message_channel &ch) noexcept
    : channel(ch)
{
}

auto sync_socket_sink::log(const log_context &ctx) const noexcept -> void
try {
    channel.get().send_log(ctx);
} catch (...) {
    thread_local fmt::memory_buffer buf;
    buf.clear();
    format_log(buf, ctx, output_format::text, { });

    const utils::file_descriptor stderr_fd{ STDERR_FILENO, false };
    auto bytes = utils::as_bytes(utils::span(buf.data(), buf.size()));
    std::ignore = stderr_fd.write_span(bytes);
}

} // namespace linyaps_box::log
