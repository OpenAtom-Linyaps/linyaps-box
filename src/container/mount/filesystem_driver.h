/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
    explicit OverlayfsFuseFilesystemDriver(util::strVec lowerDirs, std::string upperDir, std::string workDir,
                                           std::string mountPoint);

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
    explicit FuseProxyFilesystemDriver(util::strVec mounts, std::string mountPoint);

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
