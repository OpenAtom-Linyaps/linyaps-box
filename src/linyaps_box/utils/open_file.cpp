// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/open_file.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"

#include <asm/unistd.h>

#ifdef LINYAPS_BOX_HAVE_OPENAT2_H
#include <linux/openat2.h>
#endif

#include <unistd.h>

#ifndef RESOLVE_IN_ROOT
#define RESOLVE_IN_ROOT 0x10
#endif

#ifndef __NR_openat2
#define __NR_openat2 437
#endif

namespace {
linyaps_box::utils::file_descriptor
open_at_fallback(const linyaps_box::utils::file_descriptor &root,
                 const std::filesystem::path &path,
                 int flag,
                 int mode)
{
    LINYAPS_BOX_DEBUG() << "fallback openat " << path.c_str() << " at FD=" << root.get() << " with "
                        << linyaps_box::utils::inspect_fcntl_or_open_flags(flag) << "\n\t"
                        << linyaps_box::utils::inspect_fd(root.get());
    // TODO: we need implement a compatible fallback
    // currently we just use openat and do some simple check
    auto file_path = path.relative_path();
    int fd = ::openat(root.get(), file_path.c_str(), flag, mode);
    if (fd < 0) {
        auto full_path = root.current_path() / path.relative_path();
        throw std::system_error(errno,
                                std::generic_category(),
                                std::string{ "openat: failed to open " } + full_path.string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

linyaps_box::utils::file_descriptor
syscall_openat2(int dirfd, const char *path, uint64_t flag, uint64_t mode, uint64_t resolve)
{
    struct openat2_how
    {
        uint64_t flags;
        uint64_t mode;
        uint64_t resolve;
    } how{ flag, mode, resolve };

    auto ret = syscall(__NR_openat2, dirfd, path, &how, sizeof(openat2_how), 0);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "openat2");
    }

    return linyaps_box::utils::file_descriptor{ static_cast<int>(ret) };
}
} // namespace

linyaps_box::utils::file_descriptor linyaps_box::utils::open(const std::filesystem::path &path,
                                                             int flag,
                                                             mode_t mode)
{
    LINYAPS_BOX_DEBUG() << "open " << path.c_str() << " with " << inspect_fcntl_or_open_flags(flag);
    int fd = ::open(path.c_str(), flag, mode);
    if (fd == -1) {
        throw std::system_error(errno,
                                std::generic_category(),
                                "open: failed to open " + path.string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}

linyaps_box::utils::file_descriptor
linyaps_box::utils::open_at(const linyaps_box::utils::file_descriptor &root,
                            const std::filesystem::path &path,
                            int flag,
                            mode_t mode)
{
    LINYAPS_BOX_DEBUG() << "open " << path.c_str() << " at FD=" << root.get() << " with "
                        << inspect_fcntl_or_open_flags(flag) << "\n\t" << inspect_fd(root.get());

    static bool support_openat2{ true };
    while (support_openat2) {
        try {
            return syscall_openat2(root.get(), path.c_str(), flag, mode, RESOLVE_IN_ROOT);
        } catch (const std::system_error &e) {
            auto code = e.code().value();
            if (code == EINTR || code == EAGAIN) {
                continue;
            }

            if (code == ENOSYS) {
                support_openat2 = false;
                break;
            }

            if (code == EINVAL || code == EPERM) {
                break;
            }

            throw std::system_error(
                    code,
                    std::generic_category(),
                    std::string{ e.what() } + ": failed to open "
                            + (root.current_path() / path.relative_path()).string());
        }
    }

    // NOTE: openat2 only available after linux 5.15
    return open_at_fallback(root, path, flag, mode);
}
