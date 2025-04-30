// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <filesystem>

#include <fcntl.h>

namespace linyaps_box::utils {

file_descriptor open(const std::filesystem::path &path, int flag = O_RDONLY, mode_t mode = 0644);

file_descriptor open_at(const file_descriptor &root,
                        const std::filesystem::path &path,
                        int flag = O_RDONLY,
                        mode_t mode = 0644);

} // namespace linyaps_box::utils
