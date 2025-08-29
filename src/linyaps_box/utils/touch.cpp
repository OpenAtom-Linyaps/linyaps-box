// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/touch.h"

#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"

#include <fcntl.h>

auto linyaps_box::utils::touch(const file_descriptor &root,
                               const std::filesystem::path &path,
                               int flag,
                               mode_t mode) -> linyaps_box::utils::file_descriptor
{
    LINYAPS_BOX_DEBUG() << "touch " << path << " at " << inspect_fd(root.get());
    const auto fd = ::openat(root.get(), path.c_str(), flag, mode);
    if (fd == -1) {
        throw std::system_error(errno,
                                std::system_category(),
                                "openat: " + (root.current_path() / path.relative_path()).string());
    }

    return linyaps_box::utils::file_descriptor{ fd };
}
