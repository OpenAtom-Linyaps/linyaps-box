// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/mkdir.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

auto linyaps_box::utils::mkdir(const file_descriptor &root, std::filesystem::path path, mode_t mode)
        -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "mkdir " << path << " at " << inspect_fd(root.get());

    auto current = root.duplicate();
    if (path.empty()) {
        // do nothing
        return current;
    }

    if (path.is_absolute()) {
        path = path.lexically_relative("/");
    }

    if (path.filename().empty()) {
        path = path.parent_path();
    }

    int fd{ -1 };
    int depth{ 0 };
    for (const auto &part : path) {
        LINYAPS_BOX_DEBUG() << "part=" << part << " mode=0" << std::oct << mode;

        if (part == "..") {
            --depth;
        }

        if (depth < 0) {
            return current;
        }

        if (::mkdirat(current.get(), part.c_str(), mode) != 0 && errno != EEXIST) {
            LINYAPS_BOX_DEBUG() << "current path: " << utils::inspect_path(current.get())
                                << " perm:" << utils::inspect_permissions(current.get());
            throw std::system_error(errno,
                                    std::generic_category(),
                                    "mkdirat: failed to create "
                                            + (current.current_path() / part).string());
        }

        fd = ::openat(current.get(), part.c_str(), O_PATH);
        if (fd == -1) {
            throw std::system_error(errno,
                                    std::generic_category(),
                                    "openat: failed to open "
                                            + (current.current_path() / part).string());
        }

        current = file_descriptor(fd);
        ++depth;
    }

    return current;
}
