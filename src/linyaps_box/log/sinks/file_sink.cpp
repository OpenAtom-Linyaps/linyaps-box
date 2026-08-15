// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/file_sink.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/utils/span.h"

#include <fmt/format.h>

#include <fcntl.h>

namespace linyaps_box::log {

file_sink::file_sink(file_spec spec, output_format fmt)
    : fd(std::move(spec.fd))
    , format_(fmt)
{
}

auto file_sink::log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void
try {
    format_log(buf, ctx, format_, { });

    auto bytes = utils::as_bytes(utils::span(buf.data(), buf.size()));
    std::ignore = fd.write_span(bytes);
} catch (...) { // NOLINT
    // swallow
}

} // namespace linyaps_box::log
