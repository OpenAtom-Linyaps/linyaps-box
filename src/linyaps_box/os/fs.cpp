// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/fs.h"

#include "linyaps_box/os/syscall_nr.h"
#include "linyaps_box/utils/utils.h"

#include <sys/stat.h>

#ifdef LINYAPS_BOX_HAVE_OPENAT2_H
#  include <linux/openat2.h>
#endif

#include <cstddef>
#include <cstring>
#include <optional>

#include <dirent.h>

namespace {

auto to_dir_entry_file_type(unsigned char d_type) noexcept -> std::filesystem::file_type
{
    switch (d_type) {
    case DT_REG:
        return std::filesystem::file_type::regular;
    case DT_DIR:
        return std::filesystem::file_type::directory;
    case DT_LNK:
        return std::filesystem::file_type::symlink;
    case DT_BLK:
        return std::filesystem::file_type::block;
    case DT_CHR:
        return std::filesystem::file_type::character;
    case DT_FIFO:
        return std::filesystem::file_type::fifo;
    case DT_SOCK:
        return std::filesystem::file_type::socket;
    case DT_UNKNOWN:
    default:
        return std::filesystem::file_type::unknown;
    }
}

} // namespace

namespace linyaps_box::os {

namespace sys {
auto open_option::from_raw(unsigned int raw) noexcept -> Result<open_option>
{
    auto mode{ access_mode::read_only };
    if ((raw & O_PATH) != 0) {
        mode = access_mode::path;
    } else {
        const auto val{ raw & acc_mask };
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

    const auto flag = open_flag(
      (mode == access_mode::path) ? (raw & ~static_cast<unsigned int>(O_PATH)) : (raw & ~acc_mask));
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

        return unexpected{ make_error_code(errno) };
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

        return unexpected{ make_error_code(errno) };
    }
}

#if defined(O_LARGEFILE) && (O_LARGEFILE != 0)
static_assert((sys::open_how{ sys::open_option{ sys::open_flag::none, sys::access_mode::read_only },
                              std::filesystem::perms::none,
                              utils::bitflags<sys::openat2_resolve>{ } }
                 .native_flags()
               & static_cast<std::uint64_t>(O_LARGEFILE))
                != 0,
              "openat2 must request O_LARGEFILE on descriptors opened for I/O");
static_assert((sys::open_how{ sys::open_option{ sys::open_flag::none, sys::access_mode::path },
                              std::filesystem::perms::none,
                              utils::bitflags<sys::openat2_resolve>{ } }
                 .native_flags()
               & static_cast<std::uint64_t>(O_LARGEFILE))
                == 0,
              "O_PATH must not carry O_LARGEFILE (openat2 rejects it with EINVAL)");
#endif

auto openat2(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             const sys::open_how &how) noexcept -> Result<utils::file_descriptor>
{
    struct
    {
        uint64_t flags;
        uint64_t mode;
        uint64_t resolve;
    } linux_open_how{
        how.native_flags(),
        static_cast<uint64_t>(how.perms),
        how.resolve.to_raw(),
    };

    while (true) {
        const auto fd = static_cast<int>(::syscall(nr_openat2,
                                                   dirfd.get(),
                                                   path.c_str(),
                                                   &linux_open_how,
                                                   sizeof(linux_open_how)));
        if (LIKELY(fd >= 0)) {
            return utils::file_descriptor{ fd };
        }

        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}

auto unlinkat(utils::file_descriptor_ref dirfd,
              const std::filesystem::path &path,
              utils::bitflags<sys::at_flag> flags) noexcept -> Result<void>
{
    if (UNLIKELY(::unlinkat(dirfd, path.c_str(), flags.to_raw()) < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto mkdirat(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             std::filesystem::perms perm) noexcept -> Result<void>
{
    if (UNLIKELY(::mkdirat(dirfd, path.c_str(), static_cast<mode_t>(perm)) < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto symlinkat(const std::filesystem::path &target,
               utils::file_descriptor_ref newdirfd,
               const std::filesystem::path &linkpath) noexcept -> Result<void>
{
    if (UNLIKELY(::symlinkat(target.c_str(), newdirfd, linkpath.c_str()) < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto renameat2(utils::file_descriptor_ref olddirfd,
               const std::filesystem::path &oldpath,
               utils::file_descriptor_ref newdirfd,
               const std::filesystem::path &newpath,
               utils::bitflags<sys::rename_flag> flags) noexcept -> Result<void>
{
    if (UNLIKELY(::renameat2(olddirfd, oldpath.c_str(), newdirfd, newpath.c_str(), flags.to_raw())
                 < 0)) {

        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto fstat(utils::file_descriptor_ref fd) noexcept -> Result<struct stat>
{
    struct stat st{ };
    if (UNLIKELY(::fstat(fd, &st) < 0)) {

        return unexpected{ make_error_code(errno) };
    }

    return st;
}

auto fstatat(utils::file_descriptor_ref dirfd,
             const std::filesystem::path &path,
             utils::bitflags<sys::at_flag> flags) noexcept -> Result<struct stat>
{
    struct stat st{ };
    if (UNLIKELY(::fstatat(dirfd, path.c_str(), &st, flags.to_raw()) < 0)) {

        return unexpected{ make_error_code(errno) };
    }

    return st;
}

auto statx(utils::file_descriptor_ref fd,
           const std::filesystem::path &path,
           utils::bitflags<sys::at_flag> flags,
           utils::bitflags<sys::statx_flag> mask) noexcept -> Result<struct statx>
{
    struct statx stx{ };
    if (UNLIKELY(::statx(fd, path.c_str(), flags.to_raw(), mask.to_raw(), &stx) == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return stx;
}

auto readlinkat(utils::file_descriptor_ref dirfd,
                const std::filesystem::path &path,
                std::size_t hint) noexcept -> Result<std::string>
{
    if (hint < 256) {
        std::array<char, 256> stack_buf; // NOLINT
        const auto n = ::readlinkat(dirfd, path.c_str(), stack_buf.data(), stack_buf.size());
        if (LIKELY(n >= 0 && static_cast<std::size_t>(n) < stack_buf.size())) {
            stack_buf[n] = 0;

            return std::string(stack_buf.data());
        }

        if (UNLIKELY(n < 0)) {
            return unexpected{ make_error_code(errno) };
        }
    }

    auto cap = hint == 0 ? 512 : std::max(hint + 1, static_cast<std::size_t>(512));
    std::string buf;
    while (true) {
        buf.resize(cap);

        const auto n = ::readlinkat(dirfd, path.c_str(), buf.data(), cap);
        if (UNLIKELY(n < 0)) {
            return unexpected{ make_error_code(errno) };
        }

        if (static_cast<std::size_t>(n) < cap) {
            buf.resize(n);
            return buf;
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

        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto linkat(utils::file_descriptor_ref olddirfd,
            const std::filesystem::path &oldpath,
            utils::file_descriptor_ref newdirfd,
            const std::filesystem::path &newpath,
            utils::bitflags<sys::at_flag> flags) noexcept -> Result<void>
{
    if (UNLIKELY(::linkat(olddirfd, oldpath.c_str(), newdirfd, newpath.c_str(), flags.to_raw())
                 < 0)) {

        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto fcntl_dupfd(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>
{
    const auto ret = ::fcntl(fd, F_DUPFD, newfd);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return utils::file_descriptor{ ret };
}

auto fcntl_dupfd_cloexec(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>
{
    const auto ret = ::fcntl(fd, F_DUPFD_CLOEXEC, newfd);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return utils::file_descriptor{ ret };
}

auto fcntl_setfl(utils::file_descriptor_ref fd, utils::bitflags<sys::open_flag> flag) noexcept
  -> Result<void>
{
    if (UNLIKELY(::fcntl(fd, F_SETFL, flag.to_raw()) < 0)) {
        return unexpected(make_error_code(errno));
    }

    return { };
}

auto fcntl_getfl(utils::file_descriptor_ref fd) noexcept -> Result<sys::open_option>
{
    const auto ret = ::fcntl(fd, F_GETFL);
    if (UNLIKELY(ret < 0)) {
        return unexpected(make_error_code(errno));
    }

    return sys::open_option::from_raw(ret);
}

auto fcntl_setfd(utils::file_descriptor_ref fd, utils::bitflags<sys::fd_flag> flag) noexcept
  -> Result<void>
{
    if (UNLIKELY(::fcntl(fd, F_SETFD, flag.to_raw()) < 0)) {

        return unexpected(make_error_code(errno));
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
    // getdents64 was added after glibc 2.30 but we need support 2.28,
    // so use syscall directly.
    auto ret = syscall(__NR_getdents64, fd.get(), buf.data(), buf.size());
    if (UNLIKELY(ret < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return ret;
}

auto fstatfs(utils::file_descriptor_ref fd) noexcept -> Result<struct statfs>
{
    struct statfs sf{ };
    if (UNLIKELY(::fstatfs(fd, &sf) < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return sf;
}

auto fchmod(utils::file_descriptor_ref fd, std::filesystem::perms perm) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchmod(fd, static_cast<mode_t>(perm)) < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return { };
    }
}

auto fchmodat(utils::file_descriptor_ref dirfd,
              const std::filesystem::path &path,
              std::filesystem::perms perm,
              utils::bitflags<sys::at_flag> flags) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchmodat(dirfd, path.c_str(), static_cast<mode_t>(perm), flags.to_raw())
                     < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return { };
    }
}

[[nodiscard]] auto fchown(utils::file_descriptor_ref fd, uid_t owner, gid_t group) noexcept
  -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchown(fd, owner, group) < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return { };
    }
}

[[nodiscard]] auto fchownat(utils::file_descriptor_ref dirfd,
                            const std::filesystem::path &path,
                            uid_t owner,
                            gid_t group,
                            utils::bitflags<sys::at_flag> flags) noexcept -> Result<void>
{
    while (true) {
        if (UNLIKELY(::fchownat(dirfd, path.c_str(), owner, group, flags.to_raw()) < 0)) {
            if (errno == EINTR) {
                continue;
            }

            return unexpected{ make_error_code(errno) };
        }

        return { };
    }
}

auto ftruncate(utils::file_descriptor_ref fd, off_t length) noexcept -> Result<void>
{
    while (true) {
        const auto ret = ::ftruncate(fd, length);
        if (LIKELY(ret == 0)) {
            return { };
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}

auto memfd_create(const std::string &name, utils::bitflags<sys::memfd_flag> flags) noexcept
  -> Result<utils::file_descriptor>
{
    const auto ret = ::memfd_create(name.c_str(), flags.to_raw());
    if (UNLIKELY(ret == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return utils::file_descriptor{ ret };
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

auto directory::fail(std::error_code ec) noexcept -> Result<std::optional<dir_entry>>
{
    error_ = ec;
    return unexpected{ error_ };
}

auto directory::next() noexcept -> Result<std::optional<dir_entry>>
{
    if (UNLIKELY(static_cast<bool>(error_))) {
        return unexpected{ error_ };
    }

    while (pos_ >= filled_) {
        auto n = getdents64(fd_, buffer_);
        if (UNLIKELY(!n)) {
            return fail(n.error());
        }

        if (*n == 0) {
            return std::optional<dir_entry>{ };
        }

        pos_ = 0;
        filled_ = *n;
    }

    if (UNLIKELY(filled_ - pos_ < sizeof(sys::linux_dirent64))) {
        return fail(std::make_error_code(std::errc::io_error));
    }

    // The kernel buffer is only byte-aligned, so load the fixed header with
    // memcpy instead of casting a misaligned linux_dirent64*.
    sys::linux_dirent64 raw{ };
    std::memcpy(&raw, buffer_.data() + pos_, sizeof(raw));
    if (UNLIKELY(raw.d_reclen < sizeof(sys::linux_dirent64) || pos_ + raw.d_reclen > filled_)) {
        return fail(std::make_error_code(std::errc::io_error));
    }

    // d_name is a flexible array that must be NUL-terminated inside the
    // record; bound the scan by d_reclen so a malformed (or forged) entry
    // cannot make us read past the buffer.
    constexpr auto name_offset = offsetof(sys::linux_dirent64, d_type) + 1;
    static_assert(name_offset < sizeof(sys::linux_dirent64));

    const char *name_ptr =
      reinterpret_cast<const char *>(buffer_.data() + pos_) + name_offset; // NOLINT
    const auto name_capacity = static_cast<std::size_t>(raw.d_reclen) - name_offset;
    const void *name_end = std::memchr(name_ptr, '\0', name_capacity);
    if (UNLIKELY(name_end == nullptr)) {
        return fail(std::make_error_code(std::errc::io_error));
    }

    pos_ += raw.d_reclen;

    return std::optional<dir_entry>{ dir_entry{
      std::string_view{ name_ptr,
                        static_cast<std::size_t>(static_cast<const char *>(name_end) - name_ptr) },
      raw.d_ino,
      to_dir_entry_file_type(raw.d_type) } };
}

} // namespace linyaps_box::os
