// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/infra/rootfs.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/utils.h"

#include <linux/magic.h>
#include <sys/statfs.h>
#include <sys/syscall.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace linyaps_box::infra {
namespace {

using os::make_error_code;
using os::Result;
using os::unexpected;
using os::sys::access_mode;
using os::sys::at_flag;
using os::sys::open_flag;
using os::sys::open_option;
using os::sys::openat2_resolve;
using file_descriptor = utils::file_descriptor;

// The emulated resolver processes one component at a
// time, so the kernel's MAXSYMLINKS (40) can be exhausted more easily than with
// in-kernel resolution.
constexpr auto max_symlinks = 128;

// Probe with the same resolve flags the real scoped open uses (RESOLVE_IN_ROOT
// | RESOLVE_NO_MAGICLINKS).
[[nodiscard]] auto probe_openat2() noexcept -> bool
{
    auto probe = os::openat2(utils::file_descriptor_ref::cwd(),
                             ".",
                             { open_option(open_flag::cloexec, access_mode::path),
                               std::filesystem::perms::none,
                               openat2_resolve::in_root | openat2_resolve::no_magic_links });
    return probe.has_value();
}

[[nodiscard]] auto same_inode(const struct stat &a, const struct stat &b) noexcept -> bool
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

[[nodiscard]] auto walk_split(const std::filesystem::path &path) -> std::vector<std::string>
{
    std::vector<std::string> parts;
    parts.reserve(8);
    for (const auto &comp : path) {
        const auto &sv = comp.native();
        if (sv == "/") {
            continue;
        }

        // Empty components (from trailing slash) are kept as "." so that
        // openat(current, ".") validates the current is a directory.
        // "." components are kept for the same reason..
        parts.emplace_back(sv.empty() ? "." : sv);
    }

    return parts;
}

[[nodiscard]] auto has_trailing_dot_or_dotdot(const std::filesystem::path &p) noexcept -> bool
{
    const auto &name = p.filename();
    return name == "." || name == "..";
}

[[nodiscard]] auto has_trailing_slash(const std::filesystem::path &p) noexcept -> bool
{
    return !p.native().empty() && p.native().back() == std::filesystem::path::preferred_separator;
}

[[nodiscard]] auto strip_trailing_slashes(const std::filesystem::path &p) noexcept
  -> std::filesystem::path
{
    std::string_view s = p.native();
    while (!s.empty() && s.back() == std::filesystem::path::preferred_separator) {
        s.remove_suffix(1);
    }

    if (s.size() != p.native().size()) {
        return std::filesystem::path{ s };
    }

    return p;
}

// Filesystems known to use nd_jump_link() for magic-links.
// Following absolute symlinks on these filesystems in userspace is unsafe.
[[nodiscard]] auto is_magiclink_filesystem(const file_descriptor &fd) noexcept -> bool
{
    struct statfs buf{ };
    if (UNLIKELY(::fstatfs(fd.get(), &buf) < 0)) {
        return false;
    }

    return buf.f_type == PROC_SUPER_MAGIC || buf.f_type == AAFS_MAGIC;
}

// Verifies via /proc/self/fd that the current fd matches the expected path
// and that the root hasn't been moved.
[[nodiscard]] auto check_current(utils::file_descriptor_ref root_fd,
                                 const std::filesystem::path &root_proc_path,
                                 utils::file_descriptor_ref current_fd,
                                 const std::filesystem::path &expected_path) noexcept
  -> Result<void>
{
    auto current_path = current_fd.current_path();
    if (UNLIKELY(current_path.empty())) {
        return unexpected{ make_error_code(ESTALE) };
    }

    auto full_expected = expected_path.empty() ? root_proc_path : (root_proc_path / expected_path);
    if (UNLIKELY(current_path != full_expected)) {
        return unexpected{ std::make_error_code(std::errc::permission_denied) };
    }

    auto now = root_fd.current_path();
    if (UNLIKELY(now.empty())) {
        return unexpected{ make_error_code(ESTALE) };
    }

    if (UNLIKELY(now != root_proc_path)) {
        return unexpected{ make_error_code(ESTALE) };
    }

    return { };
}

// resolve_scoped variant that returns the partial state on ENOENT instead of
// propagating the error.  The returned (handle, remaining) pair represents the
// deepest reachable directory and the unprocessed path components, so that
// callers like create_directories can switch to creation mode from the correct
// starting point — even when the ENOENT was triggered by a symlink target
// component rather than the original path.
struct partial_lookup
{
    file_descriptor fd;
    std::vector<std::string> remaining;
    std::error_code error;
};

// Tracks symlink entry state for resolve_partial. When a symlink is entered,
// we save the current walk state so that if resolution fails partway through
// the symlink target, we can rewind to the state before the symlink.
struct symlink_entry
{
    file_descriptor fd;
    std::vector<std::string> saved_parts;  // reversed, parts at entry (before pushing target)
    std::vector<std::string> target_parts; // forward order, symlink target components
    std::string symlink_name;              // the symlink component name (for rewind)
    std::size_t consumed{ 0 };             // how many target_parts have been walked
};

auto resolve_partial(utils::file_descriptor_ref root_fd,
                     const std::filesystem::path &path,
                     bool no_follow_trailing = false) noexcept -> Result<partial_lookup>
{
    auto parts = walk_split(path);
    if (parts.empty()) {
        auto dup = os::fcntl_dupfd_cloexec(root_fd, 0);
        if (UNLIKELY(!dup)) {
            return unexpected{ std::move(dup).error() };
        }

        return partial_lookup{ std::move(*dup), { }, { } };
    }

    const auto root_proc_path = root_fd.current_path();
    if (UNLIKELY(root_proc_path.empty())) {
        return unexpected{ make_error_code(ESTALE) };
    }

    const auto root_stat_res = os::fstat(root_fd);
    if (UNLIKELY(!root_stat_res)) {
        return unexpected{ std::move(root_stat_res).error() };
    }

    std::reverse(parts.begin(), parts.end());

    auto current_res = os::fcntl_dupfd_cloexec(root_fd, 0);
    if (!current_res) {
        return unexpected{ std::move(current_res).error() };
    }
    auto current = std::move(*current_res);
    auto current_ref = current.ref();

    std::filesystem::path expected_path;

    std::deque<symlink_entry> symlink_stack;

    auto budget{ max_symlinks };
    while (!parts.empty()) {
        auto name{ std::move(parts.back()) };
        parts.pop_back();

        if (name == "..") {
            auto cur_res = os::fstat(current_ref);
            if (UNLIKELY(!cur_res)) {
                return unexpected{ std::move(cur_res).error() };
            }

            if (same_inode(*cur_res, *root_stat_res)) {
                if (auto r = check_current(root_fd, root_proc_path, current_ref, expected_path);
                    UNLIKELY(!r)) {
                    return unexpected{ std::move(r).error() };
                }

                // ".." clamped at root — still counts as a consumed symlink target component
                if (!symlink_stack.empty()) {
                    auto &entry = symlink_stack.back();
                    if (entry.consumed < entry.target_parts.size()) {
                        ++entry.consumed;

                        if (entry.consumed == entry.target_parts.size()) {
                            symlink_stack.pop_back();
                        }
                    }
                }

                continue;
            }

            auto pres = os::openat(current_ref,
                                   "..",
                                   open_option(open_flag::no_follow, access_mode::path),
                                   std::filesystem::perms::none);
            if (UNLIKELY(!pres)) {
                return unexpected{ std::move(pres).error() };
            }

            current = std::move(*pres);
            current_ref = current.ref();
            expected_path = expected_path.parent_path();

            if (auto r = check_current(root_fd, root_proc_path, current_ref, expected_path);
                UNLIKELY(!r)) {
                return unexpected{ std::move(r).error() };
            }

            // ".." not at root — counts as a consumed symlink target component
            if (!symlink_stack.empty()) {
                auto &entry = symlink_stack.back();
                if (entry.consumed < entry.target_parts.size()) {
                    ++entry.consumed;
                    if (entry.consumed == entry.target_parts.size()) {
                        symlink_stack.pop_back();
                    }
                }
            }

            continue;
        }

        if (UNLIKELY(name.find('/') != std::string::npos)) {
            return unexpected{ std::make_error_code(std::errc::invalid_argument) };
        }

        auto comp_res = os::openat(current_ref,
                                   name,
                                   open_option(open_flag::no_follow, access_mode::path),
                                   std::filesystem::perms::none);
        if (UNLIKELY(!comp_res)) {
            if (comp_res.error() == std::errc::no_such_file_or_directory) {
                if (!symlink_stack.empty()) {
                    // Rewind to state before the outermost symlink was entered.
                    auto entry = std::move(symlink_stack.front());
                    symlink_stack.pop_front();
                    // remaining = [symlink_name] + reversed(saved_parts)
                    std::reverse(entry.saved_parts.begin(), entry.saved_parts.end());
                    entry.saved_parts.push_back(std::move(entry.symlink_name));
                    std::reverse(entry.saved_parts.begin(), entry.saved_parts.end());
                    return partial_lookup{ std::move(entry.fd),
                                           std::move(entry.saved_parts),
                                           comp_res.error() };
                }

                parts.push_back(std::move(name));
                std::reverse(parts.begin(), parts.end());
                return partial_lookup{ std::move(current), std::move(parts), comp_res.error() };
            }

            return unexpected{ std::move(comp_res).error() };
        }
        auto comp_fd = std::move(*comp_res);
        auto comp_ref = comp_fd.ref();

        auto st_res = os::fstatat(comp_ref, "", at_flag::empty_path | at_flag::symlink_nofollow);
        if (UNLIKELY(!st_res)) {
            return unexpected{ std::move(st_res).error() };
        }
        const auto &st = *st_res;

        const auto type = os::to_fs_file_type(st.st_mode);
        if (type == std::filesystem::file_type::symlink) {
            if (no_follow_trailing && parts.empty()) {
                return partial_lookup{ std::move(comp_fd), { }, { } };
            }

            if (UNLIKELY(--budget < 0)) {
                return unexpected{ std::make_error_code(std::errc::too_many_symbolic_link_levels) };
            }

            auto target_res = os::readlinkat(comp_fd.ref(), "");
            if (UNLIKELY(!target_res)) {
                return unexpected{ std::move(target_res).error() };
            }
            const auto &target = *target_res;
            LINYAPS_BOX_LOG_DEBUG("symlink -> {}", target);

            // Save walk state before entering the symlink target.
            auto dup_res = os::fcntl_dupfd_cloexec(current_ref, 0);
            if (UNLIKELY(!dup_res)) {
                return unexpected{ std::move(dup_res).error() };
            }

            auto target_parts = walk_split(target.relative_path());
            symlink_stack.push_back(
              symlink_entry{ std::move(*dup_res), parts, target_parts, name });

            if (!target.empty() && target.is_absolute()) {
                if (UNLIKELY(is_magiclink_filesystem(comp_fd))) {
                    return unexpected{ std::make_error_code(
                      std::errc::too_many_symbolic_link_levels) };
                }

                auto dup_res2 = os::fcntl_dupfd_cloexec(root_fd, 0);
                if (UNLIKELY(!dup_res2)) {
                    return unexpected{ std::move(dup_res2).error() };
                }

                current = std::move(*dup_res2);
                current_ref = current.ref();
                expected_path = "";
            }

            auto target_parts_rev = target_parts;
            std::move(target_parts_rev.rbegin(),
                      target_parts_rev.rend(),
                      std::back_inserter(parts));
            continue;
        }

        if (UNLIKELY(type != std::filesystem::file_type::directory && !parts.empty())) {
            return unexpected{ std::make_error_code(std::errc::not_a_directory) };
        }

        if (name != ".") {
            expected_path /= name;
        }
        current = std::move(comp_fd);
        current_ref = current.ref();

        // Track symlink target component consumption; pop entry when fully resolved.
        if (!symlink_stack.empty()) {
            auto &entry = symlink_stack.back();
            if (entry.consumed < entry.target_parts.size()) {
                ++entry.consumed;
                if (entry.consumed == entry.target_parts.size()) {
                    symlink_stack.pop_back();
                }
            }
        }
    }

    if (auto r = check_current(root_fd, root_proc_path, current_ref, expected_path); UNLIKELY(!r)) {
        return unexpected{ std::move(r).error() };
    }

    return partial_lookup{ std::move(current), { }, { } };
}

auto resolve_scoped(utils::file_descriptor_ref root_fd,
                    const std::filesystem::path &path,
                    bool no_follow_trailing) noexcept -> Result<file_descriptor>
{
    auto partial = resolve_partial(root_fd, path, no_follow_trailing);
    if (UNLIKELY(!partial)) {
        return unexpected{ std::move(partial).error() };
    }

    if (!partial->remaining.empty()) {
        return unexpected{ partial->error };
    }

    return std::move(partial->fd);
}

struct resolved_parent
{
    file_descriptor dir;
    std::string name; // basename
};

auto resolve_parent(utils::file_descriptor_ref root_fd, const std::filesystem::path &path)
  -> Result<resolved_parent>
{
    auto name = path.filename().string();
    auto parent = path.parent_path();
    const std::string_view sv = parent.native();
    if (sv.empty() || sv == "/" || sv == path.native()) {
        auto dup = os::fcntl_dupfd_cloexec(root_fd, 0);
        if (UNLIKELY(!dup)) {
            return unexpected{ std::move(dup).error() };
        }

        return resolved_parent{ std::move(*dup), std::move(name) };
    }

    auto dir = resolve_scoped(root_fd, parent, false);
    if (UNLIKELY(!dir)) {
        return unexpected{ std::move(dir).error() };
    }

    return resolved_parent{ std::move(*dir), std::move(name) };
}

auto remove_dirent(utils::file_descriptor_ref dirfd, const std::filesystem::path &path) noexcept
  -> Result<void>
{
    auto unlink_res = os::unlinkat(dirfd, path, at_flag::none);
    if (unlink_res) {
        return { };
    }

    auto rmdir_res = os::unlinkat(dirfd, path, at_flag::remove_dir);
    if (rmdir_res) {
        return { };
    }

    std::error_code final_err = (rmdir_res.error() == std::errc::not_a_directory)
      ? unlink_res.error()
      : std::move(rmdir_res).error();

    return unexpected{ final_err };
}

auto remove_all_in_dir(utils::file_descriptor_ref dirfd, const std::filesystem::path &name) noexcept
  -> Result<void>
{
    if (name.native().find('/') != std::string::npos) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    struct task
    {
        std::string name;
        file_descriptor dir;
        bool scanned{ false };
    };

    std::vector<task> stack;
    auto dup_res = os::fcntl_dupfd_cloexec(dirfd, 0);
    if (UNLIKELY(!dup_res)) {
        return unexpected{ std::move(dup_res).error() };
    }
    stack.push_back(task{ name.string(), std::move(*dup_res), false });

    struct alignas(std::max_align_t) dirent_buf
    {
        std::byte data[32768];
    };

    auto buf = std::make_unique<dirent_buf>();

    while (!stack.empty()) {
        auto &t = stack.back();

        // Fast path: unlink for files/symlinks, rmdir for empty directories.
        if (auto rm = remove_dirent(t.dir.ref(), t.name);
            rm || rm.error() == std::errc::no_such_file_or_directory) {
            stack.pop_back();
            continue;
        }

        if (t.scanned) {
            // Children were processed but the directory is still non-empty
            // (racing writer repopulated it). Give up.
            return unexpected{ std::make_error_code(std::errc::directory_not_empty) };
        }
        t.scanned = true;

        // Open the directory. O_NOFOLLOW prevents following a symlink
        // swapped in by a racer.
        auto subfd_res =
          os::openat(t.dir.ref(),
                     t.name,
                     open_option(open_flag::directory | open_flag::cloexec | open_flag::no_follow,
                                 access_mode::read_only),
                     std::filesystem::perms::none);
        if (UNLIKELY(!subfd_res)) {
            if (subfd_res.error() == std::errc::no_such_file_or_directory) {
                stack.pop_back();
                continue;
            }

            return unexpected{ subfd_res.error() };
        }
        auto subfd = std::move(*subfd_res);

        // Scan the directory.
        while (true) {
            auto n = os::getdents64(subfd.ref(), buf->data);
            if (UNLIKELY(!n)) {
                return unexpected(n.error());
            }

            if (*n == 0) {
                break;
            }

            for (std::size_t pos = 0; pos < *n;) {
                if (UNLIKELY(*n - pos < sizeof(os::sys::linux_dirent64))) {
                    return unexpected{ std::make_error_code(std::errc::io_error) };
                }

                const auto *d = reinterpret_cast<const os::sys::linux_dirent64 *>(buf->data + pos);
                if (UNLIKELY(d->d_reclen < sizeof(os::sys::linux_dirent64)
                             || pos + d->d_reclen > *n)) {
                    return unexpected{ std::make_error_code(std::errc::io_error) };
                }

                pos += d->d_reclen;
                auto child_name = d->name();
                if (child_name == "." || child_name == "..") {
                    continue;
                }

                auto subdup_res = os::fcntl_dupfd_cloexec(subfd.ref(), 0);
                if (UNLIKELY(!subdup_res)) {
                    return unexpected{ std::move(subdup_res).error() };
                }

                stack.push_back(task{ std::string{ child_name }, std::move(*subdup_res), false });
            }
        }
    }

    return { };
}

} // namespace

Root::Root(file_descriptor fd) noexcept
    : fd_(std::move(fd))
{
}

auto Root::valid() const noexcept -> bool
{
    return fd_.valid();
}

auto Root::fd() const noexcept -> int
{
    return fd_.get();
}

auto Root::ref() const noexcept -> utils::file_descriptor_ref
{
    return fd_.ref();
}

auto Root::open(const std::filesystem::path &rootfs) noexcept -> os::Result<Root>
{
    auto fd_res =
      os::openat(utils::file_descriptor_ref::cwd(),
                 rootfs,
                 open_option{ open_flag::directory | open_flag::cloexec, access_mode::path });
    if (UNLIKELY(!fd_res)) {
        return unexpected{ fd_res.error() };
    }

    return Root{ std::move(*fd_res) };
}

auto Root::reopen() noexcept -> os::Result<void>
{
    auto old_path = fd_.ref().current_path();
    if (UNLIKELY(old_path.empty())) {
        return unexpected{ make_error_code(ESTALE) };
    }

    auto fd_res = os::openat(utils::file_descriptor_ref::cwd(),
                             old_path,
                             open_option(open_flag::directory, access_mode::path),
                             std::filesystem::perms::none);
    if (UNLIKELY(!fd_res)) {
        return unexpected{ std::move(fd_res).error() };
    }
    auto newfd = std::move(*fd_res);

    auto new_path = newfd.ref().current_path();
    if (new_path.empty()) {
        return unexpected{ make_error_code(ESTALE) };
    }

    if (new_path != old_path) {
        return unexpected{ std::make_error_code(std::errc::permission_denied) };
    }
    fd_ = std::move(newfd);

    return { };
}

auto Root::open(const std::filesystem::path &path,
                open_option opt,
                std::filesystem::perms perm) const noexcept -> os::Result<file_descriptor>
{
    const auto flags = opt.flags();

    // O_CREAT/O_EXCL cannot be emulated by the O_PATH-based fallback (and in
    // the fallback case O_CREAT would be silently ignored).
    // Inode creation is the job of create_file/create_directories/create.
    // Reject them here so the openat2 fast path and the emulated fallback agree
    if ((flags & (open_flag::create | open_flag::exclusive)) != open_flag::none) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    static const auto openat2_supported = probe_openat2();
    if (openat2_supported) {
        auto res =
          os::openat2(fd_.ref(),
                      path,
                      { opt, perm, openat2_resolve::in_root | openat2_resolve::no_magic_links });
        if (LIKELY(res.has_value())) {
            return res;
        }

        // Only fall back when openat2 itself is unsupported (ENOSYS/EINVAL/EPERM).
        // Legitimate filesystem errors like ENOENT should be returned directly.
        const auto &ec = res.error();
        if (ec != std::errc::function_not_supported && ec != std::errc::invalid_argument
            && ec != std::errc::operation_not_permitted) {
            return unexpected{ res.error() };
        }

        // Re-probe: openat2 might have been disabled by seccomp or mount namespace change
        if (!probe_openat2()) {
            LINYAPS_BOX_LOG_DEBUG("openat2 unavailable, try fallback implement");
        } else {
            return unexpected{ res.error() };
        }
    }

    auto no_follow_trailing = (flags & open_flag::no_follow) != open_flag::none;

    auto handle_res = resolve_scoped(fd_.ref(), path, no_follow_trailing);
    if (UNLIKELY(!handle_res)) {
        return unexpected{ handle_res.error() };
    }

    // The leaf type is only ever a symlink when O_NOFOLLOW was set
    // (resolve_scoped follows trailing symlinks otherwise).
    auto st_res =
      os::fstatat(handle_res->ref(), "", at_flag::empty_path | at_flag::symlink_nofollow);
    if (UNLIKELY(!st_res)) {
        return unexpected{ st_res.error() };
    }
    auto type = os::to_fs_file_type(st_res->st_mode);

    if (type == std::filesystem::file_type::symlink) {
        // O_DIRECTORY on a symlink yields ENOTDIR (the
        // symlink is not a directory) before any O_PATH/ELOOP handling.
        if (UNLIKELY((flags & open_flag::directory) != open_flag::none)) {
            return unexpected{ std::make_error_code(std::errc::not_a_directory) };
        }

        // O_PATH|O_NOFOLLOW on a symlink returns a handle to the symlink.
        if (opt.acc_mode() == os::sys::access_mode::path) {
            return std::move(*handle_res);
        }

        return unexpected{ std::make_error_code(std::errc::too_many_symbolic_link_levels) };
    }

    // O_PATH handles don't need reopening; still apply the O_DIRECTORY
    // check the kernel would have performed on open.
    if (opt.acc_mode() == os::sys::access_mode::path) {
        if (UNLIKELY((flags & open_flag::directory) != open_flag::none
                     && type != std::filesystem::file_type::directory)) {
            return unexpected{ std::make_error_code(std::errc::not_a_directory) };
        }

        return std::move(*handle_res);
    }

    // openat does not accept AT_EMPTY_PATH,
    // so openat(fd, "") would always return ENOENT.
    // The trailing symlink is already resolved (and the leaf verified to be
    // a non-symlink above), so O_NOFOLLOW is a no-op here.
    // We must strip it anyway: /proc/self/fd/N is itself a magic-link,
    // and O_NOFOLLOW would make the reopen fail with ELOOP.
    auto reopen_flags = flags & ~open_flag::no_follow;
    return os::open(handle_res->ref().proc_path(),
                    open_option{ reopen_flags, opt.acc_mode() },
                    perm);
}

auto Root::create_directory(const std::filesystem::path &path,
                            std::filesystem::perms perm) const noexcept
  -> os::Result<file_descriptor>
{
    if (has_trailing_dot_or_dotdot(path)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }
    auto parent_res = resolve_parent(fd_.ref(), path);
    if (UNLIKELY(!parent_res)) {
        return unexpected{ std::move(parent_res).error() };
    }

    auto parent = parent_res->dir.ref();
    if (auto r = os::mkdirat(parent, parent_res->name, perm); UNLIKELY(!r)) {
        return unexpected{ std::move(r).error() };
    }

    return os::openat(parent,
                      parent_res->name,
                      open_option(open_flag::no_follow, access_mode::path));
}

auto Root::create_directories(const std::filesystem::path &path,
                              std::filesystem::perms perms) const noexcept
  -> os::Result<file_descriptor>
{
    const auto raw = static_cast<unsigned>(perms);
    // Reject file type bits, setuid, setgid — mkdirat silently ignores them
    if ((raw & ~07777) != 0 || (raw & ~01777) != 0) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto root_ref = fd_.ref();

    auto partial_res = resolve_partial(root_ref, path);
    if (UNLIKELY(!partial_res)) {
        return unexpected{ std::move(partial_res).error() };
    }
    auto &partial = *partial_res;

    // Every component resolved — reopen the fd with O_DIRECTORY and return.
    if (partial.remaining.empty()) {
        auto res =
          os::openat(partial.fd.ref(),
                     ".",
                     open_option(open_flag::directory | open_flag::cloexec, access_mode::path));
        if (UNLIKELY(!res)) {
            return unexpected{ std::move(res).error() };
        }

        return res;
    }

    // Partial resolution: create the remaining components.
    // the remaining path from a partial lookup never contains ".." because the walk would have
    // resolved it.
    auto current = std::move(partial.fd);
    for (const auto &name : partial.remaining) {
        if (UNLIKELY(name == "..")) {
            return unexpected{ std::make_error_code(std::errc::no_such_file_or_directory) };
        }

        // mkdirat does not follow trailing symlinks, so this is safe even if
        // a racing attacker swaps the parent directory.
        auto mkres = os::mkdirat(current.ref(), name, perms);
        if (UNLIKELY(!mkres && mkres.error() != std::errc::file_exists)) {
            return unexpected{ std::move(mkres).error() };
        }

        auto nfd_res =
          os::openat(current.ref(),
                     name,
                     open_option(open_flag::no_follow | open_flag::directory | open_flag::cloexec,
                                 access_mode::path));
        if (UNLIKELY(!nfd_res)) {
            return unexpected{ std::move(nfd_res).error() };
        }

        current = std::move(*nfd_res);
    }

    return current;
}

auto Root::create_file(const std::filesystem::path &path,
                       open_flag flags,
                       std::filesystem::perms perm) const noexcept -> os::Result<file_descriptor>
{
    if (has_trailing_slash(path) || has_trailing_dot_or_dotdot(path)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    if ((flags & open_flag::tmpfile) != open_flag::none) {
        // O_TMPFILE: the path is a directory, resolve it and create an
        // unnamed temporary file within it.
        auto dir_res = resolve_scoped(fd_.ref(), path, false);
        if (UNLIKELY(!dir_res)) {
            return unexpected{ std::move(dir_res).error() };
        }

        return os::openat(dir_res->ref(),
                          ".",
                          open_option{ flags | open_flag::cloexec, access_mode::read_write },
                          perm);
    }

    auto parent_res = resolve_parent(fd_.ref(), path);
    if (UNLIKELY(!parent_res)) {
        return unexpected{ std::move(parent_res).error() };
    }

    auto leaf = flags | open_flag::create | open_flag::no_follow;
    return os::openat(parent_res->dir.ref(),
                      parent_res->name,
                      open_option(leaf, access_mode::read_write),
                      perm);
}

auto Root::create(const std::filesystem::path &path, const inode_type &type) const noexcept
  -> os::Result<void>
{
    //   - Non-directory inode types with trailing slash → EINVAL.
    //   - Hardlink target with trailing slash → EINVAL.
    //   - Path "/" → EINVAL.
    if (has_trailing_slash(path) || has_trailing_dot_or_dotdot(path)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto parent_res = resolve_parent(fd_.ref(), path);
    if (UNLIKELY(!parent_res)) {
        return unexpected{ std::move(parent_res).error() };
    }
    auto dir = parent_res->dir.ref();
    const auto &name = parent_res->name;

    if (name.empty()) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    return std::visit(
      utils::Overload{
        [&](const symlink_spec &spec) -> os::Result<void> {
            return os::symlinkat(spec.target, dir, name);
        },
        [&](const hardlink_spec &spec) -> os::Result<void> {
            if (UNLIKELY(has_trailing_slash(spec.target))) {
                return unexpected{ std::make_error_code(std::errc::invalid_argument) };
            }
            auto old_res = resolve_parent(fd_.ref(), spec.target);
            if (UNLIKELY(!old_res)) {
                return unexpected{ std::move(old_res).error() };
            }
            return os::linkat(old_res->dir.ref(), old_res->name, dir, name);
        },
        [&](const fifo_spec &spec) -> os::Result<void> {
            return os::mknodat(dir, name, std::filesystem::file_type::fifo, spec.mode, 0);
        },
        [&](const character_device_spec &spec) -> os::Result<void> {
            return os::mknodat(dir,
                               name,
                               std::filesystem::file_type::character,
                               spec.mode,
                               spec.dev);
        },
        [&](const block_device_spec &spec) -> os::Result<void> {
            return os::mknodat(dir, name, std::filesystem::file_type::block, spec.mode, spec.dev);
        },
      },
      type);
}

auto Root::rename(const std::filesystem::path &source,
                  const std::filesystem::path &destination,
                  os::sys::rename_flag flag) const noexcept -> os::Result<void>
{
    auto ref = fd_.ref();

    //   - Source trailing slash implies source must be a directory.
    //   - Destination trailing slash (non-RENAME_EXCHANGE) implies source must
    //     be a directory.
    //   - RENAME_EXCHANGE requires both paths to exist.
    const auto src_trailing = has_trailing_slash(source);
    const auto dst_trailing = has_trailing_slash(destination);
    if (has_trailing_dot_or_dotdot(source) || has_trailing_dot_or_dotdot(destination)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto clean_src = src_trailing ? source.parent_path() : source;

    auto olddir_res = resolve_parent(ref, clean_src);
    if (UNLIKELY(!olddir_res)) {
        return unexpected{ std::move(olddir_res).error() };
    }

    // Resolve source once and reuse for both trailing-slash and exchange checks.
    // Only resolve when needed — the common case (simple rename) doesn't need it.
    std::optional<file_descriptor> src_handle;
    auto need_src = src_trailing
      || (flag & os::sys::rename_flag::exchange) != os::sys::rename_flag::none || dst_trailing;
    if (need_src) {
        auto src_res = resolve_scoped(ref, clean_src, false);
        if (UNLIKELY(!src_res)) {
            return unexpected{ std::move(src_res).error() };
        }

        src_handle = std::move(*src_res);
    }

    if (src_trailing || (flag & os::sys::rename_flag::exchange) != os::sys::rename_flag::none) {
        if (src_trailing) {
            auto st_res =
              os::fstatat(src_handle->ref(), "", at_flag::empty_path | at_flag::symlink_nofollow);

            if (UNLIKELY(!st_res)) {
                return unexpected{ std::move(st_res).error() };
            }

            if (auto type = os::to_fs_file_type(st_res->st_mode);
                type != std::filesystem::file_type::directory) {
                return unexpected{ std::make_error_code(std::errc::not_a_directory) };
            }
        }
    }

    auto newdir_res = resolve_parent(ref, destination);
    if (UNLIKELY(!newdir_res)) {
        return unexpected{ std::move(newdir_res).error() };
    }

    if (dst_trailing && (flag & os::sys::rename_flag::exchange) == os::sys::rename_flag::none) {
        auto st_res =
          os::fstatat(src_handle->ref(), "", at_flag::empty_path | at_flag::symlink_nofollow);
        if (UNLIKELY(!st_res)) {
            return unexpected{ std::move(st_res).error() };
        }

        if (auto type = os::to_fs_file_type(st_res->st_mode);
            type != std::filesystem::file_type::directory) {
            return unexpected{ std::make_error_code(std::errc::not_a_directory) };
        }
    }

    return os::renameat2(olddir_res->dir.ref(),
                         olddir_res->name,
                         newdir_res->dir.ref(),
                         newdir_res->name,
                         flag);
}

auto Root::remove_inode(const std::filesystem::path &path, bool is_dir) const noexcept
  -> Result<void>
{
    // trailing slash on a non-directory remove → ENOTDIR.
    if (has_trailing_slash(path) && !is_dir) {
        return unexpected{ std::make_error_code(std::errc::not_a_directory) };
    }

    if (has_trailing_dot_or_dotdot(path)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto clean = strip_trailing_slashes(path);

    auto name = clean.filename();
    if (!is_dir && name.empty()) {
        return unexpected{ std::make_error_code(std::errc::not_a_directory) };
    }

    auto flag = is_dir ? at_flag::remove_dir : at_flag::none;
    auto parent_res = resolve_parent(fd_.ref(), clean);
    if (UNLIKELY(!parent_res)) {
        return unexpected{ std::move(parent_res).error() };
    }

    return os::unlinkat(parent_res->dir.ref(), parent_res->name, flag);
}

auto Root::remove_file(const std::filesystem::path &path) const noexcept -> os::Result<void>
{
    return remove_inode(path, false);
}

auto Root::remove_dir(const std::filesystem::path &path) const noexcept -> os::Result<void>
{
    return remove_inode(path, true);
}

auto Root::remove_all(const std::filesystem::path &path) const noexcept -> os::Result<void>
{
    if (has_trailing_dot_or_dotdot(path)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    // Reject root paths: "/", ".", ".." (with or without trailing slash)
    auto clean = strip_trailing_slashes(path);
    const auto &sv = clean.native();
    if (sv.empty() || sv == "/" || sv == "." || sv == "..") {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto parent_res = resolve_parent(fd_.ref(), clean);
    if (UNLIKELY(!parent_res)) {
        return unexpected{ std::move(parent_res).error() };
    }
    auto parent = std::move(parent_res->dir);
    auto name = parent_res->name;

    if (name.find('/') != std::string::npos) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    return remove_all_in_dir(parent.ref(), name);
}

auto Root::readlink(const std::filesystem::path &path) const noexcept
  -> os::Result<std::filesystem::path>
{
    auto handle_res = resolve_scoped(fd_.ref(), path, true);
    if (UNLIKELY(!handle_res)) {
        return unexpected{ std::move(handle_res).error() };
    }

    return os::readlinkat(handle_res->ref(), "");
}

} // namespace linyaps_box::infra
