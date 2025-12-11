// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/vfs.h>

#include <filesystem>

#include <fcntl.h>
#include <sys/stat.h>

namespace linyaps_box::utils {

template<typename... Args>
auto fcntl(const file_descriptor &fd, int operation, Args... args) -> unsigned int
{
    auto ret = ::fcntl(fd.get(), operation, std::forward<Args>(args)...);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "fcntl");
    }

    return ret;
}

auto open(const std::filesystem::path &path, int flag = O_PATH | O_CLOEXEC, mode_t mode = 0)
        -> file_descriptor;

auto open_at(const file_descriptor &root,
             const std::filesystem::path &path,
             int flag = O_PATH | O_CLOEXEC,
             mode_t mode = 0) -> file_descriptor;

auto touch(const file_descriptor &root,
           const std::filesystem::path &path,
           int flag,
           mode_t mode = 0644) -> file_descriptor;

auto fstat(const file_descriptor &fd) -> struct stat;

auto fstatat(const file_descriptor &fd, const std::filesystem::path &path, int flag) -> struct stat;

auto fstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat;

auto lstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat;

auto lstat(const std::filesystem::path &path) -> struct stat;

auto statfs(const file_descriptor &fd) -> struct statfs;

auto to_linux_file_type(std::filesystem::file_type type) noexcept -> int;

auto to_fs_file_type(mode_t type) noexcept -> std::filesystem::file_type;

auto is_type(mode_t mode, std::filesystem::file_type type) noexcept -> bool;

auto is_type(mode_t mode, mode_t type) noexcept -> bool;

auto to_string(std::filesystem::file_type type) noexcept -> std::string_view;

} // namespace linyaps_box::utils
