// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

namespace linyaps_box::utils {

std::string inspect_fcntl_or_open_flags(size_t flags);
std::string inspect_fd(int fd);
std::string inspect_fds();
std::string inspect_permissions(int fd);
std::filesystem::path inspect_path(int fd);

} // namespace linyaps_box::utils
