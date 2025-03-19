// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/statfs.h>

#include <fcntl.h>
#include <sys/stat.h>

namespace linyaps_box::utils {

inline struct stat fstatat(const file_descriptor &fd, std::filesystem::path path, int flag = 0)
{
    if (!path.empty() && path.is_absolute()) {
        path = path.lexically_relative("/");
    }

    struct stat statbuf{};
    auto ret = ::fstatat(fd.get(), path.c_str(), &statbuf, flag);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "fstatat");
    }

    return statbuf;
}

inline struct stat fstatat(const file_descriptor &fd, const std::filesystem::path &path = "")
{
    return linyaps_box::utils::fstatat(fd, path, AT_EMPTY_PATH);
}

inline struct stat lstatat(const file_descriptor &fd, const std::filesystem::path &path = "")
{
    return linyaps_box::utils::fstatat(fd, path, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
}

inline struct statfs statfs(const file_descriptor &fd)
{
    struct statfs statbuf{};
    auto ret = ::statfs(fd.proc_path().c_str(), &statbuf);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "statfs");
    }

    return statbuf;
}

} // namespace linyaps_box::utils
