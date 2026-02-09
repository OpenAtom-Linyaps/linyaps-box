// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"

#include <sys/syscall.h>

#include <cassert>

#ifdef LINYAPS_BOX_HAVE_OPENAT2_H
#include <linux/openat2.h>
#endif

#include <unistd.h>

#ifndef RESOLVE_IN_ROOT
#define RESOLVE_IN_ROOT 0x10
#endif

#ifndef __NR_openat2
#define __NR_openat2 437
#endif

namespace {
auto open_at_fallback(const linyaps_box::utils::file_descriptor &root,
                      const std::filesystem::path &path,
                      int flag,
                      mode_t mode) -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "fallback openat " << path.c_str() << " at FD=" << root.get() << " with "
                        << linyaps_box::utils::inspect_fcntl_or_open_flags(
                                   static_cast<size_t>(flag))
                        << "\n\t" << linyaps_box::utils::inspect_fd(root.get());
    // TODO: we need implement a compatible fallback
    // currently we just use openat and do some simple check
    const auto &file_path = path.relative_path();
    const auto fd = ::openat(root.get(), file_path.c_str(), flag, mode);
    if (fd < 0) {
        auto full_path = root.current_path() / path.relative_path();
        throw std::system_error(errno,
                                std::system_category(),
                                std::string{ "openat: failed to open " } + full_path.string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

auto syscall_openat2(int dirfd, const char *path, uint64_t flag, uint64_t mode, uint64_t resolve)
        -> linyaps_box::utils::file_descriptor
{
    struct openat2_how
    {
        uint64_t flags;
        uint64_t mode;
        uint64_t resolve;
    } how{ flag, mode, resolve };

    const auto ret = syscall(__NR_openat2, dirfd, path, &how, sizeof(openat2_how), 0);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "openat2");
    }

    return linyaps_box::utils::file_descriptor{ static_cast<int>(ret) };
}

} // namespace

namespace linyaps_box::utils {

auto open(const std::filesystem::path &path, int flag, mode_t mode)
        -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "open " << path.c_str() << " with "
                        << inspect_fcntl_or_open_flags(static_cast<size_t>(flag));
    const auto fd = ::open(path.c_str(), flag, mode);
    if (fd == -1) {
        throw std::system_error(errno,
                                std::system_category(),
                                "open: failed to open " + path.string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

auto open_at(const linyaps_box::utils::file_descriptor &root,
             const std::filesystem::path &path,
             int flag,
             mode_t mode) -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "open " << path.c_str() << " at FD=" << root.get() << " with "
                        << inspect_fcntl_or_open_flags(static_cast<size_t>(flag)) << "\n\t"
                        << inspect_fd(root.get());

    static bool support_openat2{ true };
    while (support_openat2) {
        try {
            return syscall_openat2(root.get(),
                                   path.c_str(),
                                   static_cast<uint64_t>(flag),
                                   mode,
                                   RESOLVE_IN_ROOT);
        } catch (const std::system_error &e) {
            const auto code = e.code().value();
            if (code == EINTR || code == EAGAIN) {
                continue;
            }

            if (code == ENOSYS) {
                support_openat2 = false;
                break;
            }

            if (code == EINVAL || code == EPERM) {
                break;
            }

            throw std::system_error(
                    code,
                    std::system_category(),
                    std::string{ e.what() } + ": failed to open "
                            + (root.current_path() / path.relative_path()).string());
        }
    }

    // NOTE: openat2 only available after linux 5.15
    return open_at_fallback(root, path, flag, mode);
}

auto touch(const file_descriptor &root, const std::filesystem::path &path, int flag, mode_t mode)
        -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "touch " << path << " at " << inspect_fd(root.get());
    const auto fd = ::openat(root.get(), path.c_str(), flag, mode);
    if (fd == -1) {
        throw std::system_error(errno,
                                std::system_category(),
                                "openat: " + (root.current_path() / path.relative_path()).string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

auto fstat(const file_descriptor &fd) -> struct stat
{
    struct stat statbuf{};
    auto ret = ::fstat(fd.get(), &statbuf);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "fstat");
    }

    return statbuf;

}

auto fstatat(const file_descriptor &fd, const std::filesystem::path &path, int flag) -> struct stat

{
    struct stat statbuf{};
    auto ret = ::fstatat(fd.get(), path.c_str(), &statbuf, flag);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "fstatat");
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

auto lstat(const std::filesystem::path &path) -> struct stat

{
    struct stat statbuf{};
    auto ret = ::lstat(path.c_str(), &statbuf);
    if (ret == -1) {
        throw std::system_error(errno,
                                std::system_category(),
                                "lstat " + path.string() + " failed:");
    }

    return statbuf;

}

auto statfs(const file_descriptor &fd) -> struct statfs

{
    struct statfs statbuf{};
    auto ret = ::statfs(fd.proc_path().c_str(), &statbuf);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "statfs");
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
    switch (type & S_IFMT) {
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

    __builtin_unreachable();
}

} // namespace linyaps_box::utils
