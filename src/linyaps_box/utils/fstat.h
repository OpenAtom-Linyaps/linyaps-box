// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/statfs.h>

#include <fcntl.h>
#include <sys/stat.h>

namespace linyaps_box::utils {
auto fstatat(const file_descriptor &fd, std::filesystem::path path, int flag) -> struct stat;
auto fstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat;
auto lstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat;
auto statfs(const file_descriptor &fd) -> struct statfs;
} // namespace linyaps_box::utils
