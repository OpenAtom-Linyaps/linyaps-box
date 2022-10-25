/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_CONTAINER_MOUNT_FILESYSTEM_DRIVER_H_
#define LINGLONG_BOX_SRC_CONTAINER_MOUNT_FILESYSTEM_DRIVER_H_

#include "util/util.h"

namespace linglong {

class FilesystemDriver
{
public:
    virtual int setup() = 0;
    virtual int CreateDestinationPath(const util::fs::path &container_destination_path) = 0;
    virtual util::fs::path HostPath(const util::fs::path &container_destination_path) const = 0;
    virtual util::fs::path HostSource(const util::fs::path &container_destination_path) const = 0;
};

class OverlayfsFuseFilesystemDriver : public FilesystemDriver
{
public:
    explicit OverlayfsFuseFilesystemDriver(util::strVec lower_dirs, std::string upper_dir, std::string work_dir,
                                           std::string mount_point);

    int setup() override;

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    util::strVec lower_dirs_;
    std::string upper_dir_;
    std::string work_dir_;
    std::string mount_point_;
};

class FuseProxyFilesystemDriver : public FilesystemDriver
{
public:
    explicit FuseProxyFilesystemDriver(util::strVec mounts, std::string mount_point);

    int setup() override;

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    util::strVec mounts_;
    std::string mount_point_;
};

class NativeFilesystemDriver : public FilesystemDriver
{
public:
    explicit NativeFilesystemDriver(std::string root_path);
    ~NativeFilesystemDriver();

    int setup() override { return 0; }

    int CreateDestinationPath(const util::fs::path &container_destination_path) override;

    util::fs::path HostPath(const util::fs::path &dest_full_path) const override;

    util::fs::path HostSource(const util::fs::path &dest_full_path) const override;

private:
    std::string root_path_;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_MOUNT_FILESYSTEM_DRIVER_H_ */
