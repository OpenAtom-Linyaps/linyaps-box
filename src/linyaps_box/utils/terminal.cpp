// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "linyaps_box/utils/terminal.h"

#include "linyaps_box/utils/platform.h"

namespace linyaps_box::utils {

auto ptsname(const file_descriptor &pts) -> std::filesystem::path
{
    // The most of linux distros has /dev/pts
    auto buf_len = get_path_max("/dev/pts") + 1;
    std::string buf(buf_len, '\0');

    auto ret = ::ptsname_r(pts.get(), buf.data(), buf_len - 1);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "ptsname_r");
    }

    return std::filesystem::path{ buf.data() };
}

auto unlockpt(const file_descriptor &pt) -> void
{
    auto ret = ::unlockpt(pt.get());
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "unlockpt");
    }
}

auto tcgetattr(const file_descriptor &fd, struct termios &termios) -> void
{
    auto ret = ::tcgetattr(fd.get(), &termios);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "tcgetattr");
    }
}

auto tcsetattr(const file_descriptor &fd, int action, const struct termios &termios) -> void
{
    auto ret = ::tcsetattr(fd.get(), action, &termios);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "tcsetattr");
    }
}

auto isatty(const file_descriptor &fd) -> bool
{
    auto ret = ::isatty(fd.get());
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "isatty");
    }

    return ret == 1;
}

auto fileno(FILE *stream) -> int
{
    auto ret = ::fileno(stream);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "fileno");
    }

    return ret;
}

} // namespace linyaps_box::utils
