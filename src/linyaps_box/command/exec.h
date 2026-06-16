// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/command/options.h"

namespace linyaps_box::command {

[[nodiscard]] auto exec(exec_options options, const global_options &global) noexcept -> int;

} // namespace linyaps_box::command
