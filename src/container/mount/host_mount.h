/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_
#define LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_

#include "util/oci_runtime.h"

namespace linglong {

class FilesystemDriver;
class HostMountPrivate;

class HostMount
{
public:
    HostMount();
    ~HostMount();

    int setup(FilesystemDriver *driver);

    int mountNode(const Mount &mount);

private:
    std::unique_ptr<HostMountPrivate> hostMountPrivate;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_MOUNT_HOST_MOUNT_H_ */
