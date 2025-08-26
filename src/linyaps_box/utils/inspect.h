// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

namespace linyaps_box::utils {

auto inspect_fcntl_or_open_flags(size_t flags) -> std::string;
auto inspect_fd(int fd) -> std::string;
auto inspect_fds() -> std::string;
auto inspect_permissions(int fd) -> std::string;
auto inspect_path(int fd) -> std::filesystem::path;

} // namespace linyaps_box::utils
