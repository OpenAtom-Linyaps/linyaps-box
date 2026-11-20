// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/io.h"

#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/syscall_nr.h"
#include "linyaps_box/utils/utils.h"

#include <sys/syscall.h>

#include <array>
#include <charconv>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

namespace linyaps_box::os {

namespace {

auto syscall_close_range(unsigned int first,
                         unsigned int last,
                         utils::bitflags<sys::close_range_flag> flags) noexcept -> Result<void>
{
    if (UNLIKELY(::syscall(nr_close_range, first, last, static_cast<unsigned int>(flags.to_raw()))
                 < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto close_range_fallback(unsigned int first,
                          unsigned int last,
                          utils::bitflags<sys::close_range_flag> flags) noexcept -> Result<void>
{
    // CLOSE_RANGE_UNSHARE asks the kernel to duplicate the fd table, there is no
    // way to emulate that from userspace.
    if (flags.contains(sys::close_range_flag::unshare)) {
        return unexpected{ std::make_error_code(std::errc::operation_not_supported) };
    }

    // FIXME: verify that /proc is a real procfs and that this is our task's fd
    // directory, that check will be shared across the project.
    auto dir_fd = open("/proc/self/fd",
                       sys::open_option{ sys::open_flag::directory | sys::open_flag::cloexec,
                                         sys::access_mode::read_only });
    if (UNLIKELY(!dir_fd)) {
        return unexpected{ dir_fd.error() };
    }

    const auto self_fd = dir_fd->get();
    const bool set_cloexec = flags.contains(sys::close_range_flag::cloexec);

    std::array<std::byte, 4096> buf{ };
    directory dir{ dir_fd->ref(), buf };

    while (true) {
        auto next = dir.next();
        if (UNLIKELY(!next)) {
            return unexpected{ next.error() };
        }

        if (!next->has_value()) {
            break;
        }

        const auto name = next->value().name();
        if (name == "." || name == "..") {
            continue;
        }

        int fd{ 0 };
        const auto [end, ec] = std::from_chars(name.data(), name.data() + name.size(), fd);
        if (UNLIKELY(ec != std::errc{ } || end != name.data() + name.size())) {
            continue;
        }

        if (fd < 0 || fd == self_fd) {
            continue;
        }

        const auto ufd = static_cast<unsigned int>(fd);
        if (ufd < first || ufd > last) {
            continue;
        }

        if (set_cloexec) {
            if (UNLIKELY(::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)) {
                return unexpected{ make_error_code(errno) };
            }
        } else if (UNLIKELY(::close(fd) < 0)) {
            return unexpected{ make_error_code(errno) };
        }
    }

    return { };
}

} // namespace

auto close_range(unsigned int first,
                 unsigned int last,
                 utils::bitflags<sys::close_range_flag> flags) noexcept -> Result<void>
{
    if (UNLIKELY(first > last)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    if (UNLIKELY(!utils::bitflags<sys::close_range_flag>::from_raw(flags.to_raw()).has_value())) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    auto res = syscall_close_range(first, last, flags);
    if (LIKELY(res.has_value())) {
        return { };
    }

    const auto ec = res.error();
    if (ec == std::errc::function_not_supported     // old kernel
        || ec == std::errc::operation_not_permitted // blocked by seccomp
        || ec == std::errc::invalid_argument) {
        return close_range_fallback(first, last, flags);
    }

    return unexpected{ ec };
}

auto read(utils::file_descriptor_ref fd, utils::span<std::byte> buf) noexcept -> Result<std::size_t>
{
    while (true) {
        auto ret = ::read(fd, buf.data(), buf.size());
        if (LIKELY(ret >= 0)) {
            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}

auto write(utils::file_descriptor_ref fd, utils::span<const std::byte> buf) noexcept
  -> Result<std::size_t>
{
    while (true) {
        auto ret = ::write(fd, buf.data(), buf.size());
        if (LIKELY(ret >= 0)) {
            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }
}
} // namespace linyaps_box::os
