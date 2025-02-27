// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/command/options.h"

namespace linyaps_box::command {

int run(const std::filesystem::path &root, const run_options &opts);

} // namespace linyaps_box::command
