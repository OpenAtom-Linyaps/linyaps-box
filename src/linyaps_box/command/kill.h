// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/command/options.h"

namespace linyaps_box::command {

[[nodiscard]] auto kill(const kill_options &options, const global_options &global) -> int;

} // namespace linyaps_box::command