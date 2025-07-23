// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <filesystem>

namespace linyaps_box::utils {

void symlink(const std::filesystem::path &target, const std::filesystem::path &link_path);

void symlink_at(const std::filesystem::path &target,
                const file_descriptor &dirfd,
                const std::filesystem::path &link_path);

} // namespace linyaps_box::utils
