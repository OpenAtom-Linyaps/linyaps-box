// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/fs.h"

#include "linyaps_box/utils/utils.h"

#include <sys/sysmacros.h>

#include <memory>

#include <unistd.h>

#ifdef LINYAPS_BOX_HAVE_OPENAT2_H
#  include <linux/openat2.h>
#endif

constexpr auto openat2_sys =
#ifndef __NR_openat2
  437;
#else
  __NR_openat2;
#endif

#include <sys/syscall.h>

namespace linyaps_box::os {

namespace sys {
auto open_option::from_raw(uint raw) noexcept -> Result<open_option>
{
    auto mode{ access_mode::unknown };
    if ((raw & O_PATH) != 0) {
        mode = access_mode::path;
    } else {
        auto val{ raw & acc_mask };
        switch (val) {
        case O_RDONLY: {
            mode = access_mode::read_only;
        } break;
        case O_WRONLY: {
            mode = access_mode::write_only;
        } break;
        case O_RDWR: {
            mode = access_mode::read_write;
        } break;
        default:
            return unexpected(std::make_error_code(std::errc::invalid_argument));
        }
    }

    auto flag = open_flag((mode == access_mode::path) ? (raw & ~static_cast<uint>(O_PATH))
                                                      : (raw & ~acc_mask));
    return open_option(flag, mode);
}
} // namespace sys

auto open(const std::filesystem::path &path,
          sys::open_option opt,
          std::filesystem::perms perm) noexcept -> Result<utils::file_descriptor>
{
    while (true) {
        const auto fd =
          ::open(path.c_str(), static_cast<int>(opt.to_native()), static_cast<mode_t>(perm));
        if (LIKELY(fd >= 0)) {
            return utils::file_descriptor{ fd };
        }

        if (UNLIKELY(errno == EINTR)) {
            continue;
        }

        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }
}

auto openat(utils::file_descriptor_ref dirfd,
            const std::filesystem::path &path,
            sys::open_option opt,
            std::filesystem::perms perm) noexcept -> Result<utils::file_descriptor>
{
    while (true) {
        const auto fd = ::openat(dirfd,
                                 path.c_str(),
                                 static_cast<int>(opt.to_native()),
                                 static_cast<mode_t>(perm));
        if (LIKELY(fd >= 0)) {
            return utils::file_descriptor{ fd };
        }

        if (UNLIKELY(errno == EINTR)) {
            continue;
        }

        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }
}

auto openat2(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             const sys::open_how &how) noexcept -> Result<utils::file_descriptor>
{
    struct
    {
        uint64_t flags;
        uint64_t mode;
        uint64_t resolve;
    } linux_open_how{ static_cast<uint64_t>(how.opt),
                      static_cast<uint64_t>(how.perms),
                      static_cast<uint64_t>(how.resolve) };

    while (true) {
        const auto fd = static_cast<int>(
          ::syscall(openat2_sys, dirfd, path.c_str(), &linux_open_how, sizeof(linux_open_how)));
        if (LIKELY(fd >= 0)) {
            return utils::file_descriptor{ fd };
        }

        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }

        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }
}

auto unlinkat(utils::file_descriptor_ref dirfd,
              const std::filesystem::path &path,
              sys::at_flag flags) noexcept -> Result<void>
{
    if (UNLIKELY(::unlinkat(dirfd, path.c_str(), static_cast<int>(flags)) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto mkdirat(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             std::filesystem::perms perm) noexcept -> Result<void>
{
    if (UNLIKELY(::mkdirat(dirfd, path.c_str(), static_cast<mode_t>(perm)) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto symlinkat(const std::filesystem::path &target,
               utils::file_descriptor_ref newdirfd,
               const std::filesystem::path &linkpath) noexcept -> Result<void>
{
    if (UNLIKELY(::symlinkat(target.c_str(), newdirfd, linkpath.c_str()) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto renameat2(utils::file_descriptor_ref olddirfd,
               const std::filesystem::path &oldpath,
               utils::file_descriptor_ref newdirfd,
               const std::filesystem::path &newpath,
               sys::rename_flag flags) noexcept -> Result<void>
{
    if (UNLIKELY(
          ::renameat2(olddirfd, oldpath.c_str(), newdirfd, newpath.c_str(), static_cast<int>(flags))
          < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto fstat(utils::file_descriptor_ref fd) noexcept -> Result<struct stat>
{
    struct stat st{ };
    if (UNLIKELY(::fstat(fd, &st) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return st;
}

auto fstatat(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             sys::at_flag flags) noexcept -> Result<struct stat>
{
    struct stat st{ };
    if (UNLIKELY(::fstatat(dirfd, path.c_str(), &st, static_cast<int>(flags)) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return st;
}

auto readlinkat(utils::file_descriptor_ref dirfd,
                const std::filesystem::path &path,
                std::size_t hint) noexcept -> Result<std::filesystem::path>
{
    if (hint < 256) {
        std::array<char, 256> stack_buf{ };
        const auto n = ::readlinkat(dirfd, path.c_str(), stack_buf.data(), stack_buf.size());
        if (LIKELY(n >= 0 && static_cast<std::size_t>(n) < stack_buf.size())) {
            return std::filesystem::path(
              std::string_view(stack_buf.data(), static_cast<std::size_t>(n)));
        }

        if (UNLIKELY(n < 0)) {
            return unexpected{ make_error_code(errno) };
        }
    }

    auto cap = hint == 0 ? 512 : std::max(hint + 1, static_cast<std::size_t>(512));
    while (true) {
        auto buf = std::make_unique<char[]>(cap);
        const auto n = ::readlinkat(dirfd, path.c_str(), buf.get(), cap);
        if (UNLIKELY(n < 0)) {
            return unexpected{ make_error_code(errno) };
        }

        if (static_cast<std::size_t>(n) < cap) {
            return std::filesystem::path(std::string_view(buf.get(), static_cast<std::size_t>(n)));
        }

        if (UNLIKELY(cap > std::numeric_limits<std::size_t>::max() / 2)) {
            return unexpected{ std::make_error_code(std::errc::filename_too_long) };
        }

        cap *= 2;
    }
}

auto mknodat(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             std::filesystem::file_type type,
             std::filesystem::perms perm,
             dev_t dev) noexcept -> Result<void>
{
    mode_t type_mask{ 0 };
    switch (type) {
    case std::filesystem::file_type::regular: {
        type_mask = S_IFREG;
    } break;
    case std::filesystem::file_type::block: {
        type_mask = S_IFBLK;
    } break;
    case std::filesystem::file_type::character: {
        type_mask = S_IFCHR;
    } break;
    case std::filesystem::file_type::fifo: {
        type_mask = S_IFIFO;
    } break;
    case std::filesystem::file_type::directory:
    case std::filesystem::file_type::symlink:
    case std::filesystem::file_type::socket:
    case std::filesystem::file_type::none:
    case std::filesystem::file_type::not_found:
    case std::filesystem::file_type::unknown:
        [[fallthrough]];
    default:
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    const mode_t mode = type_mask | static_cast<mode_t>(perm);
    if (UNLIKELY(::mknodat(dirfd, path.c_str(), mode, dev) < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto linkat(utils::file_descriptor_ref olddirfd,
            const std::filesystem::path &oldpath,
            utils::file_descriptor_ref newdirfd,
            const std::filesystem::path &newpath,
            sys::at_flag flags) noexcept -> Result<void>
{
    if (UNLIKELY(
          ::linkat(olddirfd, oldpath.c_str(), newdirfd, newpath.c_str(), static_cast<int>(flags))
          < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return { };
}

auto fcntl_dupfd(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>
{
    auto ret = ::fcntl(fd, F_DUPFD, newfd);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return utils::file_descriptor{ ret };
}

auto fcntl_dupfd_cloexec(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>
{
    auto ret = ::fcntl(fd, F_DUPFD_CLOEXEC, newfd);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return utils::file_descriptor{ ret };
}

auto fcntl_setfl(utils::file_descriptor_ref fd, sys::open_flag flag) noexcept -> Result<void>
{
    if (UNLIKELY(::fcntl(fd, F_SETFL, fmt::underlying(flag)) < 0)) {
        const auto err = errno;
        return unexpected(make_error_code(err));
    }

    return { };
}

auto fcntl_getfl(utils::file_descriptor_ref fd) noexcept -> Result<sys::open_option>
{
    auto ret = ::fcntl(fd, F_GETFL);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return sys::open_option::from_raw(ret);
}

auto fcntl_setfd(utils::file_descriptor_ref fd, sys::fd_flag flag) noexcept -> Result<void>
{
    if (UNLIKELY(::fcntl(fd, F_SETFD, fmt::underlying(flag)) < 0)) {
        const auto err = errno;
        return unexpected(make_error_code(err));
    }

    return { };
}

auto fcntl_getfd(utils::file_descriptor_ref fd) noexcept -> Result<sys::fd_flag>
{
    auto ret = ::fcntl(fd, F_GETFD);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return sys::fd_flag(ret);
}

auto getdents64(utils::file_descriptor_ref fd, utils::span<std::byte> buf) noexcept
  -> Result<std::size_t>
{
    // getdents64 was added after glibc 2.30 but we need support 2.28.
    // so use syscall directly
    auto ret = syscall(SYS_getdents64, fd, buf.data(), buf.size());
    if (UNLIKELY(ret < 0)) {
        const auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return ret;
}

auto fstatfs(utils::file_descriptor_ref fd) noexcept -> Result<struct statfs>
{
    struct statfs sf{ };
    if (UNLIKELY(::fstatfs(fd, &sf) < 0)) {
        auto err = errno;
        return unexpected{ make_error_code(err) };
    }

    return sf;
}

auto fchmod(utils::file_descriptor_ref fd, std::filesystem::perms perm) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchmod(fd, static_cast<mode_t>(perm)) < 0)) {
            const auto err = errno;
            if (err == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(err) };
        }

        return { };
    }
}

auto fchmodat(utils::file_descriptor_ref dirfd,
              const std::filesystem::path &path,
              std::filesystem::perms perm,
              sys::at_flag flags) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(
              ::fchmodat(dirfd, path.c_str(), static_cast<mode_t>(perm), static_cast<int>(flags))
              < 0)) {
            const auto err = errno;
            if (err == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(err) };
        }

        return { };
    }
}

[[nodiscard]] auto fchown(utils::file_descriptor_ref fd, uid_t owner, gid_t group) noexcept
  -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchown(fd, owner, group) < 0)) {
            const auto err = errno;
            if (err == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(err) };
        }

        return { };
    }
}

[[nodiscard]] auto fchownat(utils::file_descriptor_ref dirfd,
                            const std::filesystem::path &path,
                            uid_t owner,
                            gid_t group,
                            sys::at_flag flags) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchownat(dirfd, path.c_str(), owner, group, static_cast<int>(flags)) < 0)) {
            const auto err = errno;
            if (err == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(err) };
        }

        return { };
    }
}

auto to_fs_file_type(mode_t val) noexcept -> std::filesystem::file_type
{
    switch (val & S_IFMT) {
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

} // namespace linyaps_box::os
