// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_formatter.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/utils.h"

#include <sys/statfs.h>

#include <cstdint>
#include <filesystem>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace linyaps_box::os {

constexpr auto default_dir_perm = std::filesystem::perms::owner_all
  | std::filesystem::perms::group_read | std::filesystem::perms::group_exec
  | std::filesystem::perms::others_read | std::filesystem::perms::others_exec;

constexpr auto default_file_perm = std::filesystem::perms::none;

constexpr auto default_new_file_perm = std::filesystem::perms::owner_read
  | std::filesystem::perms::owner_write | std::filesystem::perms::group_read
  | std::filesystem::perms::others_read;

namespace sys {

enum class open_flag : uint64_t { // NOLINT
    none = 0,
    create = O_CREAT,
    exclusive = O_EXCL,
    truncate = O_TRUNC,
    append = O_APPEND,
    non_block = O_NONBLOCK,
    cloexec = O_CLOEXEC,
    directory = O_DIRECTORY,
    direct = O_DIRECT,
    sync = O_SYNC,
    no_follow = O_NOFOLLOW,
    async = O_ASYNC,
    dsync = O_DSYNC,
    largefile = O_LARGEFILE,
    no_atime = O_NOATIME,
    no_ctty = O_NOCTTY,
    tmpfile = O_TMPFILE,
    LINYAPS_MARK_AS_BITMASK_ENUM(tmpfile),
};
// O_TMPFILE contains O_DIRECTORY, so it must be placed at first.
// O_LARGEFILE is 0 on 64-bit platforms (kernel always supports large files),
// so only register it in the name table when it's a real non-zero flag.
#if O_LARGEFILE
#  define LINYAPS_BOX_OPEN_FLAG_LARGEFILE_ENTRY { open_flag::largefile, "O_LARGEFILE" },
#else
#  define LINYAPS_BOX_OPEN_FLAG_LARGEFILE_ENTRY
#endif
LINYAPS_REGISTER_ENUM(open_flag,
                      { open_flag::none, "NONE" },
                      { open_flag::tmpfile, "O_TMPFILE" },
                      { open_flag::sync, "O_SYNC" },
                      { open_flag::create, "O_CREAT" },
                      { open_flag::exclusive, "O_EXCL" },
                      { open_flag::cloexec, "O_CLOEXEC" },
                      { open_flag::truncate, "O_TRUNC" },
                      { open_flag::append, "O_APPEND" },
                      { open_flag::non_block, "O_NONBLOCK" },
                      { open_flag::directory, "O_DIRECTORY" },
                      { open_flag::no_follow, "O_NOFOLLOW" },
                      { open_flag::async, "O_ASYNC" },
                      { open_flag::direct, "O_DIRECT" },
                      { open_flag::dsync, "O_DSYNC" },
                      LINYAPS_BOX_OPEN_FLAG_LARGEFILE_ENTRY{ open_flag::no_atime, "O_NOATIME" },
                      { open_flag::no_ctty, "O_NOCTTY" })
#undef LINYAPS_BOX_OPEN_FLAG_LARGEFILE_ENTRY

enum class access_mode : mode_t { // NOLINT
    unknown,
    read_only,
    write_only,
    read_write,
    path
};

constexpr auto format_as(access_mode mode) noexcept -> std::string_view
{
    switch (mode) {
    case access_mode::unknown:
        return "Unknown";
    case access_mode::read_only:
        return "O_RDONLY";
    case access_mode::write_only:
        return "O_WRONLY";
    case access_mode::read_write:
        return "O_RDWR";
    case access_mode::path:
        return "O_PATH";
    }
}

class open_option
{
public:
    constexpr static uint acc_mask = O_ACCMODE;
    open_option() = delete;

    static auto from_raw(uint raw) noexcept -> Result<open_option>;

    constexpr open_option(open_flag flags, access_mode acc_mode) noexcept
        : flags_(flags)
        , access_mode_(acc_mode)
    {
    }

    open_option(const open_option &) = default;
    open_option(open_option &&) = default;
    open_option &operator=(const open_option &) = default;
    open_option &operator=(open_option &&) = default;
    ~open_option() = default;

    [[nodiscard]] constexpr auto acc_mode() const noexcept { return access_mode_; }

    [[nodiscard]] constexpr auto flags() const noexcept { return flags_; }

    [[nodiscard]] constexpr auto to_native() const noexcept
    {
        uint mode{ 0 };
        switch (access_mode_) {
        case access_mode::read_only: {
            mode |= O_RDONLY;
        } break;
        case access_mode::write_only: {
            mode |= O_WRONLY;
        } break;
        case access_mode::read_write: {
            mode |= O_RDWR;
        } break;
        case access_mode::path: {
            mode |= O_PATH;
        } break;
        default:
            mode |= 0U;
        }

        return static_cast<uint>(flags_) | mode;
    }

    [[nodiscard]] constexpr explicit operator int() const noexcept
    {
        return static_cast<int>(to_native());
    }

    [[nodiscard]] constexpr explicit operator uint64_t() const noexcept
    {
        return static_cast<uint64_t>(to_native());
    }

private:
    open_flag flags_;
    access_mode access_mode_;
};

enum class fd_flag : uint8_t {
    none = 0,
    cloexec = FD_CLOEXEC,
    LINYAPS_MARK_AS_BITMASK_ENUM(cloexec),
};
LINYAPS_REGISTER_ENUM(fd_flag, { fd_flag::none, "NONE" }, { fd_flag::cloexec, "FD_CLOEXEC" })

enum class at_flag : std::uint16_t {
    none = 0,
    empty_path = AT_EMPTY_PATH,
    symlink_nofollow = AT_SYMLINK_NOFOLLOW,
    remove_dir = AT_REMOVEDIR,
    LINYAPS_MARK_AS_BITMASK_ENUM(empty_path),
};
LINYAPS_REGISTER_ENUM(at_flag,
                      { at_flag::none, "NONE" },
                      { at_flag::empty_path, "AT_EMPTY_PATH" },
                      { at_flag::symlink_nofollow, "AT_SYMLINK_NOFOLLOW" },
                      { at_flag::remove_dir, "AT_REMOVEDIR" })

enum class rename_flag : std::uint8_t {
    none = 0,
    exchange = RENAME_EXCHANGE,
    noreplace = RENAME_NOREPLACE,
    whiteout = RENAME_WHITEOUT,
    LINYAPS_MARK_AS_BITMASK_ENUM(whiteout),
};
LINYAPS_REGISTER_ENUM(rename_flag,
                      { rename_flag::none, "NONE" },
                      { rename_flag::exchange, "RENAME_EXCHANGE" },
                      { rename_flag::noreplace, "RENAME_NOREPLACE" },
                      { rename_flag::whiteout, "RENAME_WHITEOUT" })

enum class openat2_resolve : std::uint64_t { // NOLINT
    none = 0,

    no_xdev =
#ifdef RESOLVE_NO_XDEV
      RESOLVE_NO_XDEV
#else
      0x01
#endif
    ,

    no_magic_links =
#ifdef RESOLVE_NO_MAGICLINKS
      RESOLVE_NO_MAGICLINKS
#else
      0x02
#endif
    ,

    no_symlinks =
#ifdef RESOLVE_NO_SYMLINKS
      RESOLVE_NO_SYMLINKS
#else
      0x04
#endif
    ,

    beneath =
#ifdef RESOLVE_BENEATH
      RESOLVE_BENEATH
#else
      0x08
#endif
    ,

    in_root =
#ifdef RESOLVE_IN_ROOT
      RESOLVE_IN_ROOT
#else
      0x10
#endif
    ,

    // we couldn't stimulate RESOLVE_CACHED in userspace

    LINYAPS_MARK_AS_BITMASK_ENUM(in_root),
};
LINYAPS_REGISTER_ENUM(openat2_resolve,
                      { openat2_resolve::no_xdev, "RESOLVE_NO_XDEV" },
                      { openat2_resolve::no_magic_links, "RESOLVE_NO_MAGICLINKS" },
                      { openat2_resolve::no_symlinks, "RESOLVE_NO_SYMLINKS" },
                      { openat2_resolve::beneath, "RESOLVE_BENEATH" },
                      { openat2_resolve::in_root, "RESOLVE_IN_ROOT" })

struct open_how
{
    open_option opt;
    std::filesystem::perms perms;
    openat2_resolve resolve;
};

struct linux_dirent64
{
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;

    [[nodiscard]] std::string_view name() const noexcept // null-terminated
    {
        const char *name_ptr =
          reinterpret_cast<const char *>(this) + offsetof(linux_dirent64, d_type) + 1;
        return { name_ptr };
    }
};

} // namespace sys

[[nodiscard]] auto open(const std::filesystem::path &path,
                        sys::open_option opt,
                        std::filesystem::perms perm = default_file_perm) noexcept
  -> Result<utils::file_descriptor>;

[[nodiscard]] auto openat(utils::file_descriptor_ref dirfd,
                          const std::filesystem::path &path,
                          sys::open_option opt,
                          std::filesystem::perms perm = default_file_perm) noexcept
  -> Result<utils::file_descriptor>;

[[nodiscard]] auto openat2(utils::file_descriptor_ref dirfd,
                           const std::filesystem::path &path,
                           const sys::open_how &how) noexcept -> Result<utils::file_descriptor>;

[[nodiscard]] auto unlinkat(utils::file_descriptor_ref dirfd,
                            const std::filesystem::path &path,
                            sys::at_flag flags = sys::at_flag::none) noexcept -> Result<void>;

[[nodiscard]] auto mkdirat(utils::file_descriptor_ref dirfd,
                           const std::filesystem::path &path,
                           std::filesystem::perms perm = default_dir_perm) noexcept -> Result<void>;

[[nodiscard]] auto symlinkat(const std::filesystem::path &target,
                             utils::file_descriptor_ref dirfd,
                             const std::filesystem::path &linkpath) noexcept -> Result<void>;

[[nodiscard]] auto renameat2(utils::file_descriptor_ref olddirfd,
                             const std::filesystem::path &oldpath,
                             utils::file_descriptor_ref newdirfd,
                             const std::filesystem::path &newpath,
                             sys::rename_flag flags = sys::rename_flag::none) noexcept
  -> Result<void>;

[[nodiscard]] auto fstat(utils::file_descriptor_ref fd) noexcept -> Result<struct stat>;

[[nodiscard]] auto fstatat(utils::file_descriptor_ref dirfd,
                           const std::filesystem::path &path,
                           sys::at_flag flags = sys::at_flag::none) noexcept -> Result<struct stat>;

[[nodiscard]] auto readlinkat(utils::file_descriptor_ref dirfd,
                              const std::filesystem::path &path,
                              std::size_t hint = 0) noexcept -> Result<std::filesystem::path>;

[[nodiscard]] auto mknodat(utils::file_descriptor_ref dirfd,
                           const std::filesystem::path &path,
                           std::filesystem::file_type type,
                           std::filesystem::perms perm,
                           dev_t dev) noexcept -> Result<void>;

[[nodiscard]] auto linkat(utils::file_descriptor_ref olddirfd,
                          const std::filesystem::path &oldpath,
                          utils::file_descriptor_ref newdirfd,
                          const std::filesystem::path &newpath,
                          sys::at_flag flags = sys::at_flag::none) noexcept -> Result<void>;

[[nodiscard]] auto fcntl_dupfd(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>;

[[nodiscard]] auto fcntl_dupfd_cloexec(utils::file_descriptor_ref fd, int newfd) noexcept
  -> Result<utils::file_descriptor>;

[[nodiscard]] auto fcntl_setfl(utils::file_descriptor_ref fd, sys::open_flag flag) noexcept
  -> Result<void>;

[[nodiscard]] auto fcntl_getfl(utils::file_descriptor_ref fd) noexcept -> Result<sys::open_option>;

[[nodiscard]] auto fcntl_setfd(utils::file_descriptor_ref fd, sys::fd_flag flag) noexcept
  -> Result<void>;

[[nodiscard]] auto fcntl_getfd(utils::file_descriptor_ref fd) noexcept -> Result<sys::fd_flag>;

[[nodiscard]] auto getdents64(utils::file_descriptor_ref fd, utils::span<std::byte> buf) noexcept
  -> Result<std::size_t>;

[[nodiscard]] auto fstatfs(utils::file_descriptor_ref fd) noexcept -> Result<struct statfs>;

[[nodiscard]] auto fchmod(utils::file_descriptor_ref fd, std::filesystem::perms perm) noexcept
  -> Result<void>;

[[nodiscard]] auto fchmodat(utils::file_descriptor_ref dirfd,
                            const std::filesystem::path &path,
                            std::filesystem::perms perm,
                            sys::at_flag flags = sys::at_flag::none) noexcept -> Result<void>;

[[nodiscard]] auto fchown(utils::file_descriptor_ref fd, uid_t owner, gid_t group) noexcept
  -> Result<void>;

[[nodiscard]] auto fchownat(utils::file_descriptor_ref dirfd,
                            const std::filesystem::path &path,
                            uid_t owner,
                            gid_t group,
                            sys::at_flag flags = sys::at_flag::symlink_nofollow) noexcept
  -> Result<void>;

auto to_fs_file_type(mode_t val) noexcept -> std::filesystem::file_type;

} // namespace linyaps_box::os

template <>
struct fmt::formatter<std::filesystem::perms>
{
    enum class Presentation : uint8_t { Symbolic, Octal, Hex, Decimal };
    Presentation presentation{ Presentation::Symbolic };

    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (it != end && *it != '}') {
            switch (*it) {
            case 's': {
                presentation = Presentation::Symbolic;
            } break;
            case 'o': {
                presentation = Presentation::Octal;
            } break;
            case 'x': {
                presentation = Presentation::Hex;
            } break;
            case 'd': {
                presentation = Presentation::Decimal;
            } break;
            default:
                throw fmt::format_error("invalid format specifier for std::filesystem::perms");
            }

            ++it;
        }
        return it;
    }

    template <typename FormatContext>
    auto format(std::filesystem::perms p, FormatContext &ctx) const
    {
        using perms = std::filesystem::perms;

        auto raw = fmt::underlying(p);
        if ((p & ~perms::mask) != perms::none) {
            return fmt::format_to(ctx.out(), "invalid: {}", raw);
        }

        auto val = fmt::underlying(p & perms::mask);

        switch (presentation) {
        case Presentation::Octal:
            return fmt::format_to(ctx.out(), "{:04o}", val);
        case Presentation::Hex:
            return fmt::format_to(ctx.out(), "{:#x}", val);
        case Presentation::Decimal:
            return fmt::format_to(ctx.out(), "{}", val);
        case Presentation::Symbolic:
        default: {
            auto has = [p](std::filesystem::perms bit) noexcept {
                return (p & bit) != std::filesystem::perms::none;
            };

            auto get_exec_char =
              [](bool has_exec, bool has_special, char upper_special) noexcept -> char {
                if (has_special) {
                    return has_exec ? static_cast<char>(upper_special + 32) : upper_special;
                }

                return has_exec ? 'x' : '-';
            };

            std::array<char, 9> str{
                // Owner
                has(perms::owner_read) ? 'r' : '-',
                has(perms::owner_write) ? 'w' : '-',
                get_exec_char(has(perms::owner_exec), has(perms::set_uid), 'S'),

                // Group
                has(perms::group_read) ? 'r' : '-',
                has(perms::group_write) ? 'w' : '-',
                get_exec_char(has(perms::group_exec), has(perms::set_gid), 'S'),

                // Others
                has(perms::others_read) ? 'r' : '-',
                has(perms::others_write) ? 'w' : '-',
                get_exec_char(has(perms::others_exec), has(perms::sticky_bit), 'T')
            };

            return std::copy(str.cbegin(), str.cend(), ctx.out());
        }
        }
    }
};

template <>
struct fmt::formatter<std::filesystem::file_type> : fmt::formatter<std::string_view>
{
    auto format(std::filesystem::file_type type, fmt::format_context &ctx) const
    {
        std::string_view name = "unknown";
        switch (type) {
        case std::filesystem::file_type::none:
            name = "none";
            break;
        case std::filesystem::file_type::not_found:
            name = "not found";
            break;
        case std::filesystem::file_type::regular:
            name = "regular";
            break;
        case std::filesystem::file_type::directory:
            name = "directory";
            break;
        case std::filesystem::file_type::symlink:
            name = "symlink";
            break;
        case std::filesystem::file_type::block:
            name = "block";
            break;
        case std::filesystem::file_type::character:
            name = "character";
            break;
        case std::filesystem::file_type::fifo:
            name = "fifo";
            break;
        case std::filesystem::file_type::socket:
            name = "socket";
            break;
        case std::filesystem::file_type::unknown:
            name = "unknown";
            break;
        }

        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<linyaps_box::os::sys::open_option>
{
    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (UNLIKELY(it != end && *it != '}')) {
            throw fmt::format_error("invalid format specifier for open_how");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const linyaps_box::os::sys::open_option &opt, FormatContext &ctx) const
    {
        return fmt::format_to(ctx.out(),
                              "open_option{{ access_mode: {}, flags: {} }}",
                              opt.acc_mode(),
                              opt.flags());
    }
};

template <>
struct fmt::formatter<linyaps_box::os::sys::open_how>
{
    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();
        if (UNLIKELY(it != end && *it != '}')) {
            throw fmt::format_error("invalid format specifier for open_how");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const linyaps_box::os::sys::open_how &how, FormatContext &ctx) const
    {
        return fmt::format_to(ctx.out(),
                              "open_how{{ option {}, perms: {}, resolve: {} }}",
                              how.opt,
                              how.perms,
                              how.resolve);
    }
};
