// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/inspect.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fcntl.h>

namespace linyaps_box::utils {

namespace {
std::string inspect_fd(const std::filesystem::path &fdinfo_path)
{
    std::stringstream ss;

    ss << std::filesystem::read_symlink("/proc/self/fd/" + fdinfo_path.stem().string());

    std::ifstream fdinfo(fdinfo_path);
    assert(fdinfo.is_open());

    std::string key;

    while (fdinfo >> key) {
        ss << " " << key << " ";
        if (key != "flags:") {
            std::string value;
            fdinfo >> value;
            ss << value;
            continue;
        }

        size_t value{ 0 };

        fdinfo >> std::oct >> value;
        ss << linyaps_box::utils::inspect_fcntl_or_open_flags(value);
    }

    return ss.str();
}
} // namespace

std::string inspect_fcntl_or_open_flags(size_t flags)
{
    std::stringstream ss;

    ss << "[";
    if ((flags & O_RDONLY) != 0) {
        ss << " O_RDONLY";
    }
    if ((flags & O_WRONLY) != 0) {
        ss << " O_WRONLY";
    }
    if ((flags & O_RDWR) != 0) {
        ss << " O_RDWR";
    }
    if ((flags & O_CREAT) != 0) {
        ss << " O_CREAT";
    }
    if ((flags & O_EXCL) != 0) {
        ss << " O_EXCL";
    }
    if ((flags & O_NOCTTY) != 0) {
        ss << " O_NOCTTY";
    }
    if ((flags & O_TRUNC) != 0) {
        ss << " O_TRUNC";
    }
    if ((flags & O_APPEND) != 0) {
        ss << " O_APPEND";
    }
    if ((flags & O_NONBLOCK) != 0) {
        ss << " O_NONBLOCK";
    }
    if ((flags & O_NDELAY) != 0) {
        ss << " O_SYNC";
    }
    if ((flags & O_SYNC) != 0) {
        ss << " O_SYNC";
    }
    if ((flags & O_ASYNC) != 0) {
        ss << " O_ASYNC";
    }
    if ((flags & O_LARGEFILE) != 0) {
        ss << " O_LARGEFILE";
    }
    if ((flags & O_DIRECTORY) != 0) {
        ss << " O_DIRECTORY";
    }
    if ((flags & O_NOFOLLOW) != 0) {
        ss << " O_NOFOLLOW";
    }
    if ((flags & O_CLOEXEC) != 0) {
        ss << " O_CLOEXEC";
    }
    if ((flags & O_DIRECT) != 0) {
        ss << " O_DIRECT";
    }
    if ((flags & O_NOATIME) != 0) {
        ss << " O_NOATIME";
    }
    if ((flags & O_PATH) != 0) {
        ss << " O_PATH";
    }
    if ((flags & O_DSYNC) != 0) {
        ss << " O_DSYNC";
    }
    if ((flags & O_TMPFILE) == O_TMPFILE) {
        ss << " O_TMPFILE";
    }
    ss << " ]";
    return ss.str();
}

std::string inspect_fd(int fd)
{
    return inspect_fd("/proc/self/fdinfo/" + std::to_string(fd));
}

std::string inspect_fds()
{
    std::stringstream ss;

    bool first_line = true;

    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fdinfo")) {
        if (entry.path() == "/proc/self/fdinfo/0" || entry.path() == "/proc/self/fdinfo/1"
            || entry.path() == "/proc/self/fdinfo/2") {
            continue;
        }
        if (!first_line) {
            ss << '\n';
        }
        first_line = false;
        ss << entry.path() << " " << inspect_fd(entry.path());
    }

    return ss.str();
}

} // namespace linyaps_box::utils
