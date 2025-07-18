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

std::string inspect_fdinfo(const std::filesystem::path &fdinfoPath)
{
    std::ifstream fdinfo(fdinfoPath);
    assert(fdinfo.is_open());

    std::stringstream ss;
    std::string key;
    while (static_cast<bool>(fdinfo >> key)) {
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

std::string inspect_fdinfo(int fd)
{
    std::stringstream ss;
    ss << linyaps_box::utils::inspect_path(fd) << " ";
    ss << inspect_fdinfo(std::filesystem::path("/proc/self/fdinfo/" + std::to_string(fd)));
    return ss.str();
}

} // namespace

namespace linyaps_box::utils {

std::string inspect_fcntl_or_open_flags(size_t flags)
{
    struct FlagInfo { size_t mask; const char* name; };
    static constexpr FlagInfo flagInfos[] = {
        { O_RDONLY,    "O_RDONLY"    },
        { O_WRONLY,    "O_WRONLY"    },
        { O_RDWR,      "O_RDWR"      },
        { O_CREAT,     "O_CREAT"     },
        { O_EXCL,      "O_EXCL"      },
        { O_NOCTTY,    "O_NOCTTY"    },
        { O_TRUNC,     "O_TRUNC"     },
        { O_APPEND,    "O_APPEND"    },
        { O_NONBLOCK,  "O_NONBLOCK"  },
        { O_NDELAY,    "O_NDELAY"    },
        { O_SYNC,      "O_SYNC"      },
        { O_ASYNC,     "O_ASYNC"     },
        { O_LARGEFILE, "O_LARGEFILE" },
        { O_DIRECTORY, "O_DIRECTORY" },
        { O_NOFOLLOW,  "O_NOFOLLOW"  },
        { O_CLOEXEC,   "O_CLOEXEC"   },
        { O_DIRECT,    "O_DIRECT"    },
        { O_NOATIME,   "O_NOATIME"   },
        { O_PATH,      "O_PATH"      },
        { O_DSYNC,     "O_DSYNC"     },
        { O_TMPFILE,   "O_TMPFILE"   }
    };

    std::stringstream ss;
    ss << "[";
    for (const auto& info : flagInfos) {
        if ((flags & info.mask) == info.mask) {
            ss << " " << info.name;
        }
    }
    ss << " ]";
    return ss.str();
}

std::string inspect_fd(int fd)
{
    return inspect_fdinfo(fd);
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

std::string inspect_permissions(int fd)
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

std::filesystem::path inspect_path(int fd)
{
    std::error_code ec;
    auto ret = std::filesystem::read_symlink("/proc/self/fd/" + std::to_string(fd), ec);
    if (ec) {
        LINYAPS_BOX_ERR() << "failed to inspect path for fd " << fd << ":" << ec.message();
    }

    return ret;
}

} // namespace linyaps_box::utils
