// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "linyaps_box/utils/file.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/symlink.h"

#include <sys/syscall.h>

#include <cassert>
#include <fstream>
#include <optional>

#ifdef LINYAPS_BOX_HAVE_OPENAT2_H
#  include <linux/openat2.h>
#endif

#include <unistd.h>

#ifndef RESOLVE_IN_ROOT
#  define RESOLVE_IN_ROOT 0x10
#endif

#ifndef __NR_openat2
#  define __NR_openat2 437
#endif

namespace {

// see: https://man7.org/linux/man-pages/man7/path_resolution.7.html
constexpr auto MAX_SYMLINK_DEPTH = 40;

auto stat_same_inode(const struct stat &a, const struct stat &b) noexcept -> bool
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

auto check_not_above_root(const linyaps_box::utils::file_descriptor &current,
                          const linyaps_box::utils::file_descriptor &root) -> void
{
    auto cur_stat = linyaps_box::utils::fstat(current);
    auto root_stat = linyaps_box::utils::fstat(root);
    if (stat_same_inode(cur_stat, root_stat)) {
        throw std::system_error(EACCES,
                                std::system_category(),
                                "path escape detected: attempted to go above root");
    }
}

auto walk_component(linyaps_box::utils::file_descriptor &dir_fd,
                    const std::string &component,
                    const linyaps_box::utils::file_descriptor &root,
                    int symlink_depth) -> void;

auto resolve_symlink_component(linyaps_box::utils::file_descriptor &dir_fd,
                               linyaps_box::utils::file_descriptor &link_fd,
                               const linyaps_box::utils::file_descriptor &root,
                               int symlink_depth) -> void
{
    if (UNLIKELY(symlink_depth > MAX_SYMLINK_DEPTH)) {
        throw std::system_error(ELOOP, std::system_category(), "too many symlinks");
    }

    auto target = linyaps_box::utils::readlinkat(link_fd, "");
    LINYAPS_BOX_DEBUG() << "resolve symlink -> " << target;

    if (target.is_absolute()) {
        dir_fd = root.duplicate();
    }

    for (const auto &part : target.relative_path()) {
        walk_component(dir_fd, part.string(), root, symlink_depth);
    }
}

auto walk_component(linyaps_box::utils::file_descriptor &dir_fd,
                    const std::string &component,
                    const linyaps_box::utils::file_descriptor &root,
                    int symlink_depth) -> void
{
    if (component == ".") {
        return;
    }

    if (component == "..") {
        check_not_above_root(dir_fd, root);
        auto fd = ::openat(dir_fd.get(), "..", O_PATH | O_CLOEXEC | O_NOFOLLOW);
        if (UNLIKELY(fd < 0)) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "openat: failed to open parent directory");
        }

        linyaps_box::utils::file_descriptor parent_fd{ fd };
        dir_fd = std::move(parent_fd);
        return;
    }

    auto fd = ::openat(dir_fd.get(), component.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW);
    if (UNLIKELY(fd < 0)) {
        throw std::system_error(errno,
                                std::system_category(),
                                std::string{ "openat: failed to open component " } + component);
    }

    linyaps_box::utils::file_descriptor child_fd{ fd };
    auto child_stat = linyaps_box::utils::fstat(child_fd);

    if (S_ISLNK(child_stat.st_mode)) {
        resolve_symlink_component(dir_fd, child_fd, root, symlink_depth + 1);
        return;
    }

    if (UNLIKELY(!S_ISDIR(child_stat.st_mode))) {
        throw std::system_error(ENOTDIR, std::system_category(), "not a directory: " + component);
    }

    dir_fd = std::move(child_fd);
}

auto open_at_fallback(const linyaps_box::utils::file_descriptor &root,
                      const std::filesystem::path &path,
                      int flag,
                      mode_t mode) -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "fallback openat " << path.c_str() << " at FD=" << root.get() << " with "
                        << linyaps_box::utils::inspect_fcntl_or_open_flags(
                             static_cast<size_t>(flag))
                        << "\n\t" << linyaps_box::utils::inspect_fd(root.get());

    auto current = root.duplicate();
    const auto &parts = path.relative_path();

    auto it = parts.begin();
    auto end = parts.end();
    size_t index{ 0 };
    auto total = std::distance(it, end);

    for (; it != end; ++it, ++index) {
        auto component = it->string();
        auto is_last = (index + 1 == static_cast<size_t>(total));

        if (component == ".") {
            continue;
        }

        if (component == "..") {
            walk_component(current, component, root, 0);
            continue;
        }

        if (is_last) {
            auto fd = ::openat(current.get(), component.c_str(), flag, mode);
            if (UNLIKELY(fd < 0)) {
                auto full_path = root.current_path() / path.relative_path();
                throw std::system_error(errno,
                                        std::system_category(),
                                        std::string{ "openat: failed to open " }
                                          + full_path.string());
            }

            return linyaps_box::utils::file_descriptor{ fd };
        }

        walk_component(current, component, root, 0);
    }

    auto fd = ::openat(current.get(), ".", flag, mode);
    if (UNLIKELY(fd < 0)) {
        throw std::system_error(errno,
                                std::system_category(),
                                "openat: failed to open resolved path");
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

// TODO: use expected for better error handling
auto syscall_openat2(int dirfd,
                     const char *path,
                     uint64_t flag,
                     uint64_t mode,
                     uint64_t resolve,
                     std::error_code &ec) noexcept
  -> std::optional<linyaps_box::utils::file_descriptor>
{
    ec.clear();

    struct openat2_how
    {
        uint64_t flags;
        uint64_t mode;
        uint64_t resolve;
    } how{ flag, mode, resolve };

    const auto ret = syscall(__NR_openat2, dirfd, path, &how, sizeof(openat2_how), 0);
    if (UNLIKELY(ret < 0)) {
        ec.assign(errno, std::system_category());
        return std::nullopt;
    }

    return linyaps_box::utils::file_descriptor{ static_cast<int>(ret) };
}

auto read_pseudo_file(const std::filesystem::path &path) -> std::string
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (UNLIKELY(!ifs)) {
        throw std::runtime_error("Can't open pseudo file " + path.string());
    }

    return { (std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>() };
}

} // namespace

namespace linyaps_box::utils {

auto open(const std::filesystem::path &path, int flag, mode_t mode)
  -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "open " << path.c_str() << " with "
                        << inspect_fcntl_or_open_flags(static_cast<size_t>(flag));
    const auto fd = ::open(path.c_str(), flag, mode);
    if (UNLIKELY(fd == -1)) {
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

    // currently box is running in single thread, so use bool is safe
    static bool support_openat2{ true };
    while (support_openat2) {
        std::error_code ec;

        auto ret = syscall_openat2(root.get(),
                                   path.c_str(),
                                   static_cast<uint64_t>(flag),
                                   mode,
                                   RESOLVE_IN_ROOT,
                                   ec);
        if (ec) {
            auto val = ec.value();
            if (val == EINTR || val == EAGAIN) {
                continue;
            }

            if (val == ENOSYS) {
                support_openat2 = false;
                break;
            }

            if (val == EINVAL || val == E2BIG) {
                // EINVAL can be an unknown flag or invalid value was specified in how.
                // E2BIG can be an extension that this kernel does not support was specified in how
                return open_at_fallback(root, path, flag, mode);
            }

            throw std::system_error(ec, "failed to openat2 " + path.string());
        }

        return std::move(*ret);
    }

    // NOTE: openat2 only available after linux 5.15
    return open_at_fallback(root, path, flag, mode);
}

auto touch(const file_descriptor &root, const std::filesystem::path &path, int flag, mode_t mode)
  -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "touch " << path << " at " << inspect_fd(root.get());
    const auto fd = ::openat(root.get(), path.c_str(), flag, mode);
    if (UNLIKELY(fd == -1)) {
        throw std::system_error(errno,
                                std::system_category(),
                                "openat: " + (root.current_path() / path.relative_path()).string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

auto fstat(const file_descriptor &fd) -> struct stat
{
    struct stat statbuf{ };
    auto ret = ::fstat(fd.get(), &statbuf);
    if (UNLIKELY(ret == -1)) {
        throw std::system_error(errno, std::system_category(), "fstat");
    }

    return statbuf;

}

auto fstatat(const file_descriptor &fd, const std::filesystem::path &path, int flag) -> struct stat

{
    struct stat statbuf{ };
    auto ret = ::fstatat(fd.get(), path.c_str(), &statbuf, flag);
    if (UNLIKELY(ret == -1)) {
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
    struct stat statbuf{ };
    auto ret = ::lstat(path.c_str(), &statbuf);
    if (UNLIKELY(ret == -1)) {
        throw std::system_error(errno,
                                std::system_category(),
                                "lstat " + path.string() + " failed:");
    }

    return statbuf;

}

auto statfs(const file_descriptor &fd) -> struct statfs

{
    struct statfs statbuf{ };
    auto ret = ::statfs(fd.proc_path().c_str(), &statbuf);
    if (UNLIKELY(ret == -1)) {
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
        LINYAPS_BOX_ERR() << "Invalid file type 'none' in to_linux_file_type";
        std::terminate();
    }
    case std::filesystem::file_type::not_found: {
        LINYAPS_BOX_ERR() << "Invalid file type 'not_found' in to_linux_file_type";
        std::terminate();
    }
    default: {
        LINYAPS_BOX_ERR() << "Unhandled file type " << static_cast<int>(type)
                          << " in to_linux_file_type";
        std::terminate();
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

auto read_all(const std::filesystem::path &path) -> std::string
{
    auto fd = open(path, O_RDONLY | O_CLOEXEC);
    auto stat = fstat(fd);
    if (stat.st_size == 0) {
        return read_pseudo_file(path);
    }

    return read_exact(fd, stat.st_size);
}

auto read_exact(const file_descriptor &fd, std::size_t size) -> std::string
{
    std::string content;
    if (size == 0) {
        return content;
    }

    content.resize(size);
    std::size_t total_bytes_read{ 0 };

    const span<std::byte> full_buffer(reinterpret_cast<std::byte *>(content.data()), size);
    while (total_bytes_read < size) {
        auto [status, bytes_read] = fd.read_span(full_buffer.subspan(total_bytes_read));

        if (status == utils::IOStatus::Success) {
            total_bytes_read += bytes_read;
            continue;
        }

        if (status == utils::IOStatus::Eof) {
            content.resize(total_bytes_read);
            throw std::runtime_error("Unexpected EOF before reading requested size: "
                                     + fd.current_path().string());
        }

        if (status == utils::IOStatus::Closed) {
            throw std::runtime_error("Connection closed by peer during read_all: "
                                     + fd.current_path().string());
        }

        if (status == utils::IOStatus::TryAgain || status == utils::IOStatus::Timeout) {
            throw std::runtime_error("Read blocking/timeout on non-ready socket: "
                                     + fd.current_path().string());
        }

        throw std::runtime_error("Failed to read file due to internal error: "
                                 + fd.current_path().string());
    }

    return content;
}

void unlink_at(const file_descriptor &root, const std::filesystem::path &path)
{
    LINYAPS_BOX_DEBUG() << "Unlink " << path << " at " << root.current_path();

    auto ret = ::unlinkat(root.get(), path.c_str(), 0);
    if (UNLIKELY(ret != 0)) {
        throw std::system_error(errno, std::system_category(), "unlinkat");
    }
}

void unlink_at(const file_descriptor &root,
               const std::filesystem::path &path,
               std::error_code &ec) noexcept
{
    LINYAPS_BOX_DEBUG() << "Unlink " << path << " at " << root.current_path();

    ec.clear();
    auto ret = ::unlinkat(root.get(), path.c_str(), 0);
    if (UNLIKELY(ret != 0)) {
        ec.assign(errno, std::system_category());
    }
}

auto rename_at(const file_descriptor &old_dir,
               const std::filesystem::path &old_path,
               const file_descriptor &new_dir,
               const std::filesystem::path &new_path) -> void
{
    LINYAPS_BOX_DEBUG() << "Rename " << old_path << " at " << old_dir.current_path() << " to "
                        << new_path << " at " << new_dir.current_path();

    auto ret = ::renameat(old_dir.get(), old_path.c_str(), new_dir.get(), new_path.c_str());
    if (UNLIKELY(ret != 0)) {
        throw std::system_error(errno, std::system_category(), "renameat");
    }
}

} // namespace linyaps_box::utils
