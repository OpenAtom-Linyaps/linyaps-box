// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/fstat.h"

#include "linyaps_box/utils/log.h"

#include <cassert>

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

auto
to_linux_file_type(std::filesystem::file_type type) noexcept -> int

{
    switch (type) {
    case std::filesystem::file_type::regular:
        return S_IFREG;
    case std::filesystem::file_type::directory:
        return S_IFDIR;
    case std::filesystem::file_type::symlink:
        return S_IFLNK;
    case std::filesystem::file_type::block:
        return S_IFBLK;
    case std::filesystem::file_type::character:
        return S_IFCHR;
    case std::filesystem::file_type::fifo:
        return S_IFIFO;
    case std::filesystem::file_type::socket:
        return S_IFSOCK;
    case std::filesystem::file_type::unknown: {
        LINYAPS_BOX_WARNING() << "Try to convert unknown type to linux file type";
        return 0;
    }
    case std::filesystem::file_type::none: {
        LINYAPS_BOX_DEBUG() << "Try to convert none type to linux file type";
        assert(false);
        return -1;
    }
    case std::filesystem::file_type::not_found: {
        LINYAPS_BOX_WARNING() << "Try to convert not_found type to linux file type";
        assert(false);
        return -1;
    }
    default: {
        LINYAPS_BOX_ERR() << "Try to convert unhandled file type " << static_cast<int>(type)
                          << " to linux file type";
        assert(false);
        return -1;
    }
    }
}

auto to_fs_file_type(mode_t type) noexcept -> std::filesystem::file_type
{
    switch (type) {
    case S_IFREG:
        return std::filesystem::file_type::regular;
    case S_IFDIR:
        return std::filesystem::file_type::directory;
    case S_IFLNK:
        return std::filesystem::file_type::symlink;
    case S_IFBLK:
        return std::filesystem::file_type::block;
    case S_IFCHR:
        return std::filesystem::file_type::character;
    case S_IFIFO:
        return std::filesystem::file_type::fifo;
    case S_IFSOCK:
        return std::filesystem::file_type::socket;
    default:
        return std::filesystem::file_type::unknown;
    }
}

auto is_type(mode_t mode, std::filesystem::file_type type) noexcept -> bool
{
    auto f_type = to_linux_file_type(type);
    if (f_type <= 0) {
        return false;
    }

    return is_type(mode, f_type);
}

auto is_type(mode_t mode, mode_t type) noexcept -> bool
{
    return (mode & S_IFMT) == type;
}

auto to_string(std::filesystem::file_type type) noexcept -> std::string_view
{
    switch (type) {
    case std::filesystem::file_type::none:
        return "None";
    case std::filesystem::file_type::not_found:
        return "Not found";
    case std::filesystem::file_type::regular:
        return "Regular";
    case std::filesystem::file_type::directory:
        return "Directory";
    case std::filesystem::file_type::symlink:
        return "Symlink";
    case std::filesystem::file_type::block:
        return "Block";
    case std::filesystem::file_type::character:
        return "Character";
    case std::filesystem::file_type::fifo:
        return "FIFO";
    case std::filesystem::file_type::socket:
        return "Socket";
    case std::filesystem::file_type::unknown:
        return "Unknown";
    }
}

} // namespace linyaps_box::utils
