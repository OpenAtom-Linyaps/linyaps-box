// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/inspect.h"

#include "linyaps_box/utils/log.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <fcntl.h>
#include <sys/stat.h>

namespace {

auto inspect_fdinfo(const std::filesystem::path &fdinfoPath) -> std::string
{
    std::ifstream fdinfo(fdinfoPath);
    assert(fdinfo.is_open());

    std::stringstream ss;
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

auto inspect_fdinfo(int fd) -> std::string
{
    std::stringstream ss;
    ss << linyaps_box::utils::inspect_path(fd) << " ";
    ss << inspect_fdinfo(std::filesystem::path("/proc/self/fdinfo/" + std::to_string(fd)));
    return ss.str();
}

} // namespace

namespace linyaps_box::utils {

auto inspect_fcntl_or_open_flags(size_t flags) -> std::string
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
        ss << " O_NDELAY";
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

auto inspect_fd(int fd) -> std::string
{
    return inspect_fdinfo(fd);
}

auto inspect_fds() -> std::string
{
    std::stringstream ss;

    bool first_line = true;

    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fdinfo")) {
        if (entry.path() == "/proc/self/fdinfo/0" || entry.path() == "/proc/self/fdinfo/1"
            || entry.path() == "/proc/self/fdinfo/2") {
            continue;
        }

        auto fd = entry.path().filename();
        auto realpath = inspect_path(std::stoi(fd.c_str()));
        if (realpath.filename() == "fdinfo") {
            continue;
        }

        if (!first_line) {
            ss << '\n';
        }
        first_line = false;

        ss << entry.path() << " -> " << realpath << ":" << inspect_fdinfo(entry.path());
    }

    return ss.str();
}

auto inspect_permissions(int fd) -> std::string
{
    std::stringstream ss;
    struct stat buf{};

    if (fstat(fd, &buf) == -1) {
        throw std::system_error(errno, std::generic_category(), "fstat");
    }

    ss << buf.st_uid << ":" << buf.st_gid << " ";

    if (S_IRUSR & buf.st_mode) {
        ss << "r";
    } else {
        ss << "-";
    }

    if (S_IWUSR & buf.st_mode) {
        ss << "w";
    } else {
        ss << "-";
    }

    if (S_IXUSR & buf.st_mode) {
        ss << "x";
    } else {
        ss << "-";
    }

    if (S_IRGRP & buf.st_mode) {
        ss << "r";
    } else {
        ss << "-";
    }

    if (S_IWGRP & buf.st_mode) {
        ss << "w";
    } else {
        ss << "-";
    }

    if (S_IXGRP & buf.st_mode) {
        ss << "x";
    } else {
        ss << "-";
    }

    if (S_IROTH & buf.st_mode) {
        ss << "r";
    } else {
        ss << "-";
    }

    if (S_IWOTH & buf.st_mode) {
        ss << "w";
    } else {
        ss << "-";
    }

    if (S_IXOTH & buf.st_mode) {
        ss << "x";
    } else {
        ss << "-";
    }

    return ss.str();
}

auto inspect_path(int fd) -> std::filesystem::path
{
    std::error_code ec;
    auto ret = std::filesystem::read_symlink("/proc/self/fd/" + std::to_string(fd), ec);
    if (ec) {
        LINYAPS_BOX_ERR() << "failed to inspect path for fd " << fd << ":" << ec.message();
    }

    return ret;
}

} // namespace linyaps_box::utils
