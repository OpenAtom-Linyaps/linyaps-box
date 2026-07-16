// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/file_sink.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/utils.h"
#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/span.h"

#include <fmt/format.h>

#include <fcntl.h>

namespace linyaps_box::log {

file_sink::file_sink(const file_spec &spec)
    : fd(utils::open(spec.path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0600))
{
}

auto file_sink::log(const log_context &ctx) const -> void
try {
    fmt::memory_buffer buf;
    format_log(buf, ctx, global_logger::instance().get_format(), { });

    auto bytes = utils::as_bytes(utils::span(buf.data(), buf.size()));
    std::ignore = fd.write_span(bytes);
} catch (...) { // NOLINT
    // swallow
}

} // namespace linyaps_box::log
