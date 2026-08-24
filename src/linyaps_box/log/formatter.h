// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"

#include <fmt/color.h>
#include <fmt/format.h>

namespace linyaps_box::log {

// format_log is a convenience utility, NOT part of the sink contract.
// Sinks are free to bypass it and emit structured output directly. If
// we need more structured log utility, we could:
//
//   1. Per-sink format : each sink already stores its own
//      output_format member; the only blocker is the CLI grammar (a
//      single --log-format shared across all sinks). Extending the sink
//      spec URI, e.g. "file:out.log?format=json", unlocks mixed formats
//      with no sink code change.
//
//   2. Pluggable formatter : replace this free function + enum
//      switch with a formatter abstract base; sinks hold a
//      unique_ptr<formatter>. Worth doing only if a custom format
//      (OCI-standard, prometheus, ...) is required.
//
//   3. Structured record pipeline : widen log_context with a
//      key-value/span list so call sites can attach arbitrary fields
//      Each sink serializes the record —
//      journald already maps the fixed fields to iovec, generalize that.
//      This also dissolves the text/json dichotomy: the sink consumes a
//      record, the formatter serializes it.
auto format_log(fmt::memory_buffer &buf,
                const log_context &ctx,
                output_format fmt,
                fmt::text_style style) noexcept -> void;

} // namespace linyaps_box::log
