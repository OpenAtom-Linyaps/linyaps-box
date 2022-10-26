/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "host_mount.h"

#include <sys/stat.h>

#include <utility>

#include "filesystem_driver.h"
#include "util/debug/debug.h"

namespace linglong {

class HostMountPrivate
{
public:
    explicit HostMountPrivate() = default;

    int createDestinationPath(const util::fs::Path &containerDestinationPath) const
    {
        return filesystemDriver->createDestinationPath(containerDestinationPath);
    }

    int mountNode(const struct Mount &mount) const
    {
        int ret = -1;
        struct stat source_stat {
        };
        bool isPath = false;

        auto source = mount.source;

        if (!mount.source.empty() && mount.source[0] == '/') {
            isPath = true;
            source = filesystemDriver->hostSource(util::fs::Path(mount.source)).string();
        }

        ret = lstat(source.c_str(), &source_stat);
        if (0 == ret) {
        } else {
            // source not exist
            if (mount.fsType == Mount::kBind) {
                logErr() << "lstat" << source << "failed";
                return -1;
            }
        }

        auto destFullPath = util::fs::Path(mount.destination);
        auto destParentPath = util::fs::Path(destFullPath).parent_path();
        auto hostDestFullPath = filesystemDriver->hostPath(destFullPath);
        auto root = filesystemDriver->hostPath(util::fs::Path("/"));

        switch (source_stat.st_mode & S_IFMT) {
        case S_IFCHR: {
            filesystemDriver->createDestinationPath(destParentPath);
            std::ofstream output(hostDestFullPath.string());
            break;
        }
        case S_IFSOCK: {
            filesystemDriver->createDestinationPath(destParentPath);
            // FIXME: can not mound dbus socket on rootLess
            std::ofstream output(hostDestFullPath.string());
            break;
        }
        case S_IFLNK: {
            filesystemDriver->createDestinationPath(destParentPath);
            std::ofstream output(hostDestFullPath.string());
            source = util::fs::readSymlink(util::fs::Path(source)).string();
            break;
        }
        case S_IFREG: {
            filesystemDriver->createDestinationPath(destParentPath);
            std::ofstream output(hostDestFullPath.string());
            break;
        }
        case S_IFDIR:
            filesystemDriver->createDestinationPath(destFullPath);
            break;
        default:
            filesystemDriver->createDestinationPath(destFullPath);
            if (isPath) {
                logWan() << "unknown file type" << (source_stat.st_mode & S_IFMT) << source;
            }
            break;
        }

        auto data = util::strVecJoin(mount.data, ',');
        auto realData = data;
        auto realFlags = mount.flags;

        switch (mount.fsType) {
        case Mount::kBind:
            // make sure mount.flags always have MS_BIND
            realFlags |= MS_BIND;

            // When doing a bind mount, all flags expect MS_BIND and MS_REC are ignored by kernel.
            realFlags &= (MS_BIND | MS_REC);

            // When doing a bind mount, data and fstype are ignored by kernel. We should set them by remounting.
            realData = "";
            ret = util::fs::doMountWithFd(root.c_str(), source.c_str(), hostDestFullPath.string().c_str(), nullptr,
                                          realFlags, nullptr);
            if (0 != ret) {
                break;
            }

            if (source == "/sys") {
                sysfsIsBinded = true;
            }

            if (data.empty() && (mount.flags & ~(MS_BIND | MS_REC | MS_REMOUNT)) == 0) {
                // no need to be remounted
                break;
            }

            realFlags = mount.flags | MS_BIND | MS_REMOUNT;

            // When doing a remount, source and fstype are ignored by kernel.
            realData = data;
            ret = util::fs::doMountWithFd(root.c_str(), nullptr, hostDestFullPath.string().c_str(), nullptr, realFlags,
                                          realData.c_str());
            break;
        case Mount::kProc:
        case Mount::kDevpts:
        case Mount::kMqueue:
        case Mount::kTmpfs:
        case Mount::kSysfs:
            ret = util::fs::doMountWithFd(root.c_str(), source.c_str(), hostDestFullPath.string().c_str(),
                                          mount.type.c_str(), realFlags, realData.c_str());
            if (ret < 0) {
                // refers:
                // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L178
                // https://github.com/containers/crun/blob/38e1b5e2a3e9567ff188258b435085e329aaba42/src/libcrun/linux.c#L768-L789
                if (mount.fsType == Mount::kSysfs) {
                    realFlags = MS_BIND | MS_REC;
                    realData = "";
                    ret = util::fs::doMountWithFd(root.c_str(), "/sys", hostDestFullPath.string().c_str(), nullptr,
                                                  realFlags, nullptr);
                    if (ret == 0) {
                        sysfsIsBinded = true;
                    }
                } else if (mount.fsType == Mount::kMqueue) {
                    realFlags = MS_BIND | MS_REC;
                    realData = "";
                    ret = util::fs::doMountWithFd(root.c_str(), "/dev/mqueue", hostDestFullPath.string().c_str(),
                                                  nullptr, realFlags, nullptr);
                }
            }
            break;
        case Mount::kCgroup:
            ret = util::fs::doMountWithFd(root.c_str(), source.c_str(), hostDestFullPath.string().c_str(),
                                          mount.type.c_str(), realFlags, realData.c_str());
            // When sysfs is bind-mounted, It is ok to let cgroup mount failed.
            // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L281
            if (sysfsIsBinded) {
                ret = 0;
            }
            break;
        default:
            logErr() << "unsupported type" << mount.type;
        }

        if (EXIT_SUCCESS != ret) {
            logErr() << "mount" << source << "to" << hostDestFullPath << "failed:" << util::retErrString(ret)
                     << "\nmount args is:" << mount.type << realFlags << realData;
            if (isPath) {
                logErr() << "source file type is: 0x" << std::hex << (source_stat.st_mode & S_IFMT);
                DUMP_FILE_INFO(source);
            }
            DUMP_FILE_INFO(hostDestFullPath.string());
        }

        return ret;
    }

    std::unique_ptr<FilesystemDriver> filesystemDriver;
    mutable bool sysfsIsBinded = false;
};

HostMount::HostMount()
    : hostMountPrivate(new HostMountPrivate())
{
}

int HostMount::mountNode(const struct Mount &mount)
{
    return hostMountPrivate->mountNode(mount);
}

int HostMount::setup(FilesystemDriver *driver)
{
    if (nullptr == driver) {
        logWan() << this << hostMountPrivate->filesystemDriver.get();
        return 0;
    }

    hostMountPrivate->filesystemDriver = std::unique_ptr<FilesystemDriver>(driver);
    return hostMountPrivate->filesystemDriver->setup();
}

HostMount::~HostMount() {};

} // namespace linglong
