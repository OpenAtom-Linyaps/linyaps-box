// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/mknod.h"

#include "linyaps_box/utils/log.h"

#include <fcntl.h>
#include <sys/stat.h>

void linyaps_box::utils::mknod(const file_descriptor &root,
                               const std::filesystem::path &path,
                               mode_t mode,
                               dev_t dev)
{
    LINYAPS_BOX_DEBUG() << "Create device " << path.string() << " with mode " << mode << " and dev "
                        << dev;
    if (::mknodat(root.get(), path.c_str(), mode, dev) == 0) {
        return;
    }
    throw std::system_error(errno, std::system_category(), "mknodat");
}
