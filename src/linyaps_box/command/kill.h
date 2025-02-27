// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/command/options.h"

#include <filesystem>

namespace linyaps_box::command {

int kill(const std::filesystem::path &root, const kill_options &options);

} // namespace linyaps_box::command
