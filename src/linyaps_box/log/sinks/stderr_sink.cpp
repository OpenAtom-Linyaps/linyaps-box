// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/stderr_sink.h"

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/utils.h"
#include "linyaps_box/os/tty.h"
#include "linyaps_box/utils/span.h"

#include <fmt/color.h>
#include <fmt/format.h>

namespace linyaps_box::log {

stderr_sink::stderr_sink([[maybe_unused]] stderr_spec spec) noexcept
    : stderr_fd(STDERR_FILENO, false)
    , color(os::isatty(stderr_fd).value_or(false))
{
}

auto stderr_sink::log(const log_context &ctx) const -> void
try {
    fmt::text_style style;
    if (color) {
        switch (ctx.lvl) {
        case level::fatal: {
            style = fmt::bg(fmt::color::crimson) | fmt::fg(fmt::color::white) | fmt::emphasis::bold;
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

    thread_local fmt::memory_buffer buf;
    buf.clear();
    format_log(buf, ctx, global_logger::instance().get_format(), style);

    auto bytes = utils::as_bytes(utils::span(buf.data(), buf.size()));
    std::ignore = stderr_fd.write_span(bytes);
} catch (...) { // NOLINT
    // swallow
}

} // namespace linyaps_box::log
