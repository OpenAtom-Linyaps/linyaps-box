// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/command/options.h"

namespace linyaps_box::command {

[[nodiscard]] int list(const std::filesystem::path &root, const list_options &options);

} // namespace linyaps_box::command
