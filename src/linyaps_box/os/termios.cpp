// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/termios.h"

#include "linyaps_box/os/details/ioctl_wrapper.h"
#include "linyaps_box/utils/utils.h"

namespace linyaps_box::os {
auto isatty(utils::file_descriptor_ref fd) noexcept -> bool
{
    // we assume we're never passing an invalid file descriptor
    return ::isatty(fd) == 1;
}

auto tcgetattr(utils::file_descriptor_ref fd) noexcept -> Result<struct termios>
{
    struct termios term{ };
    if (UNLIKELY(::tcgetattr(fd, &term) == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return term;
}

auto tcsetattr(utils::file_descriptor_ref fd,
               optional_action action,
               const struct termios &termios) noexcept -> Result<void>
{
    if (UNLIKELY(::tcsetattr(fd, static_cast<int>(action), &termios) == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

auto tcsetwinsize(utils::file_descriptor_ref fd, winsize size) noexcept -> Result<void>
{
    return details::ioctl(fd, TIOCSWINSZ, &size).transform([](int) { });
}

auto tcgetwinsize(utils::file_descriptor_ref fd) noexcept -> Result<winsize>
{
    winsize size; // NOLINT
    auto ret = details::ioctl(fd, TIOCGWINSZ, &size);
    if (UNLIKELY(!ret)) {
        return unexpected{ ret.error() };
    }

    return size;
}
} // namespace linyaps_box::os
