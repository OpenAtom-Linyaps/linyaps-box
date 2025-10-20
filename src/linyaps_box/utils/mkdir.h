// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/types.h>

namespace linyaps_box::utils {

auto mkdir(const file_descriptor &root, std::filesystem::path path, mode_t mode = 0755)
        -> file_descriptor;

} // namespace linyaps_box::utils
