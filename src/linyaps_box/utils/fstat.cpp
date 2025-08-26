// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/fstat.h"

namespace linyaps_box::utils {

auto fstatat(const file_descriptor &fd, std::filesystem::path path, int flag) -> struct stat
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

auto fstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat

{
    return linyaps_box::utils::fstatat(fd, path, AT_EMPTY_PATH);

}

auto lstatat(const file_descriptor &fd, const std::filesystem::path &path) -> struct stat

{
    return linyaps_box::utils::fstatat(fd, path, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);

}

auto statfs(const file_descriptor &fd) -> struct statfs

{
    struct statfs statbuf{};
    auto ret = ::statfs(fd.proc_path().c_str(), &statbuf);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), "statfs");
    }

    return statbuf;
}
} // namespace linyaps_box::utils
