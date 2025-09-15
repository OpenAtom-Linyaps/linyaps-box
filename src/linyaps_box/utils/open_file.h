// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <filesystem>

#include <fcntl.h>

namespace linyaps_box::utils {

auto open(const std::filesystem::path &path, int flag = O_PATH | O_CLOEXEC, mode_t mode = 0)
        -> file_descriptor;

auto open_at(const file_descriptor &root,
             const std::filesystem::path &path,
             int flag = O_PATH | O_CLOEXEC,
             mode_t mode = 0) -> file_descriptor;

} // namespace linyaps_box::utils
