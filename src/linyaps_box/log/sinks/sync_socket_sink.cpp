// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/sync_socket_sink.h"

#include <sys/uio.h>
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
    // If the parent is gone, fall back to stderr
    const utils::file_descriptor stderr_fd{ STDERR_FILENO, false };
    std::string_view prefix;
    switch (ctx.lvl) {
    case level::fatal:
        prefix = "[FATAL] ";
        break;
    case level::error:
        prefix = "[ERROR] ";
        break;
    case level::warn:
        prefix = "[WARN]  ";
        break;
    case level::info:
        prefix = "[INFO]  ";
        break;
    case level::debug:
        prefix = "[DEBUG] ";
        break;
    }

    const std::array<struct iovec, 3> iov{
        { { const_cast<char *>(prefix.data()), prefix.size() },
          { const_cast<char *>(ctx.msg.data()), ctx.msg.size() },
          { const_cast<char *>("\n"), 1 } },
    };

    std::ignore = stderr_fd.write_vecs(iov); // we couldn't handle error at this point, just ignore
}

} // namespace linyaps_box::log
