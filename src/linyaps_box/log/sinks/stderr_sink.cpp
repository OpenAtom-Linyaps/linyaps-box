// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/stderr_sink.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/os/termios.h"
#include "linyaps_box/utils/span.h"

#include <fmt/color.h>
#include <fmt/format.h>

namespace linyaps_box::log {

stderr_sink::stderr_sink([[maybe_unused]] stderr_spec spec, output_format fmt) noexcept
    : stderr_fd(STDERR_FILENO, false)
    , format_(fmt)
    , color(os::isatty(stderr_fd))
{
}

auto stderr_sink::log(fmt::memory_buffer &buf, const log_context &ctx) const noexcept -> void
try {
    fmt::text_style style;
    if (color) {
        switch (ctx.lvl) {
        case level::fatal: {
            style = fmt::fg(fmt::color::white) | fmt::emphasis::bold;
        } break;
        case level::error: {
            style = fmt::fg(fmt::color::red) | fmt::emphasis::bold;
        } break;
        case level::warn: {
            style = fmt::fg(fmt::color::orange);
        } break;
        case level::info: {
            style = fmt::fg(fmt::color::cornflower_blue);
        } break;
        case level::debug: {
            style = fmt::fg(fmt::color::light_gray);
        } break;
        default:
            break;
        }
    }

    format_log(buf, ctx, format_, style);

    auto bytes = utils::as_bytes(utils::span(buf.data(), buf.size()));
    std::ignore = stderr_fd.write_span(bytes);
} catch (...) { // NOLINT
    // swallow
}

} // namespace linyaps_box::log
