// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"

#include <fmt/color.h>
#include <fmt/format.h>

namespace linyaps_box::log {

auto format_log(fmt::memory_buffer &buf,
                const log_context &ctx,
                output_format fmt,
                fmt::text_style style) noexcept -> void;

[[nodiscard]] auto get_current_format() noexcept -> output_format;

} // namespace linyaps_box::log
