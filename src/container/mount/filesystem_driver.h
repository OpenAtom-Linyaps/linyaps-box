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
    virtual int createDestinationPath(const util::fs::Path &containerDestinationPath) = 0;
    virtual util::fs::Path hostPath(const util::fs::Path &containerDestinationPath) const = 0;
    virtual util::fs::Path hostSource(const util::fs::Path &containerDestinationPath) const = 0;
};

class OverlayfsFuseFilesystemDriver : public FilesystemDriver
{
public:
    explicit OverlayfsFuseFilesystemDriver(util::strVec lowerDirs, std::string upper_dir, std::string work_dir,
                                           std::string mount_point);

    int setup() override;

    int createDestinationPath(const util::fs::Path &containerDestinationPath) override;

    util::fs::Path hostPath(const util::fs::Path &destFullPath) const override;

    util::fs::Path hostSource(const util::fs::Path &destFullPath) const override;

private:
    util::strVec lowerDirs;
    std::string upperDir;
    std::string workDir;
    std::string mountPoint;
};

class FuseProxyFilesystemDriver : public FilesystemDriver
{
public:
    explicit FuseProxyFilesystemDriver(util::strVec mounts, std::string mount_point);

    int setup() override;

    int createDestinationPath(const util::fs::Path &containerDestinationPath) override;

    util::fs::Path hostPath(const util::fs::Path &destFullPath) const override;

    util::fs::Path hostSource(const util::fs::Path &destFullPath) const override;

private:
    util::strVec mounts;
    std::string mountPoint;
};

class NativeFilesystemDriver : public FilesystemDriver
{
public:
    explicit NativeFilesystemDriver(std::string rootPath);
    ~NativeFilesystemDriver();

    int setup() override { return 0; }

    int createDestinationPath(const util::fs::Path &containerDestinationPath) override;

    util::fs::Path hostPath(const util::fs::Path &destFullPath) const override;

    util::fs::Path hostSource(const util::fs::Path &destFullPath) const override;

private:
    std::string rootPath;
};

} // namespace linglong

#endif /* LINGLONG_BOX_SRC_CONTAINER_MOUNT_FILESYSTEM_DRIVER_H_ */
