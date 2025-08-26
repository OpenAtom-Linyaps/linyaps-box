// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/close_range.h"

#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/log.h"

#include <cstring>
#include <system_error>

#include <dirent.h>
#include <fcntl.h>

namespace {
void syscall_close_range(uint fd, uint max_fd, int flags)
{
    auto ret = syscall(__NR_close_range, fd, max_fd, flags);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "close_range");
    }
}

void close_range_fallback(uint first, uint last, int flags)
{
    if ((static_cast<uint>(flags) & CLOSE_RANGE_UNSHARE) != 0) {
        // not support CLOSE_RANGE_UNSHARE
        // it request to create a new file descriptor table
        // we can't do that in user space
        throw std::runtime_error("the fallback implementation of close_range dose not support flag "
                                 "'CLOSE_RANGE_UNSHARE'");
    }

    auto *dir = opendir("/proc/self/fd");
    if (dir == nullptr) {
        throw std::system_error(errno, std::generic_category(), "opendir /proc/self/fd");
    }

    auto close_dir = make_defer([dir]() noexcept {
        if (closedir(dir) < 0) {
            LINYAPS_BOX_WARNING() << "closedir /proc/self/fd failed: " << strerror(errno)
                                  << ", but this may not be a problem";
        }
    });

    // except self fd
    // use opendir instead of std::filesystem::directory_iterator
    // because we should skip the file descriptor which is opened by opendir
    auto self_fd = dirfd(dir);
    if (self_fd < 0) {
        throw std::system_error(errno, std::generic_category(), "dirfd");
    }

    struct dirent *next{ nullptr };
    while ((next = ::readdir(dir)) != nullptr) {
        const std::string name{ next->d_name[0] };
        if (name == "." || name == "..") { // skip "." and ".."
            continue;
        }

        auto fd = std::stoi(name);
        if (fd == self_fd) {
            continue;
        }

        if (static_cast<uint>(fd) < first || static_cast<uint>(fd) > last) {
            continue;
        }

        if ((static_cast<uint>(flags) & CLOSE_RANGE_CLOEXEC) != 0) {
            if (::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                throw std::system_error(errno,
                                        std::generic_category(),
                                        "failed to set up close-on-exec to " + name);
            }
        } else {
            if (::close(fd) < 0) {
                throw std::system_error(errno, std::generic_category(), "failed to close " + name);
            }
        }
    }
}
} // namespace

void linyaps_box::utils::close_range(uint first, uint last, int flags)
{
    LINYAPS_BOX_DEBUG() << "close_range (" << first << ", " << last << ")" << "with flags "
                        << [flags]() -> std::string {
        std::stringstream ss;
        ss << '[';
        if ((static_cast<uint>(flags) & CLOSE_RANGE_CLOEXEC) != 0) {
            ss << "CLOSE_RANGE_CLOEXEC ";
        }
        if ((static_cast<uint>(flags) & CLOSE_RANGE_UNSHARE) != 0) {
            ss << "CLOSE_RANGE_UNSHARE ";
        }
        ss << ']';
        return ss.str();
    }();

    static bool support_close_range{ true };
    if (!support_close_range) {
        close_range_fallback(first, last, flags);
        return;
    }

    try {
        syscall_close_range(first, last, flags);
    } catch (const std::system_error &e) {
        auto code = e.code().value();
        if (code != ENOSYS) {
            throw;
        }

        support_close_range = false;
        close_range_fallback(first, last, flags);
    }
}
