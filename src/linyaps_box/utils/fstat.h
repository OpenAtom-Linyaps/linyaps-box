// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/statfs.h>

#include <fcntl.h>
#include <sys/stat.h>

namespace linyaps_box::utils {

inline struct stat fstat(const file_descriptor &fd)
{
    struct stat statbuf{};
    auto ret = ::fstatat(fd.get(), "", &statbuf, AT_EMPTY_PATH);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "fstatat");
    }

    return statbuf;
}

inline struct stat lstat(const file_descriptor &fd)
{
    struct stat statbuf{};
    auto ret = ::fstatat(fd.get(), "", &statbuf, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "fstatat");
    }

    return statbuf;
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
