/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <sys/wait.h>

#include <utility>

#include "filesystem_driver.h"
#include "util/platform.h"

namespace linglong {

OverlayfsFuseFilesystemDriver::OverlayfsFuseFilesystemDriver(util::strVec lowerDirs, std::string upper_dir,
                                                             std::string work_dir, std::string mount_point)
    : lowerDirs(std::move(lowerDirs))
    , upperDir(std::move(upper_dir))
    , workDir(std::move(work_dir))
    , mountPoint(std::move(mount_point))
{
}

util::fs::Path OverlayfsFuseFilesystemDriver::hostPath(const util::fs::Path &destFullPath) const
{
    return hostSource(util::fs::Path(mountPoint) / destFullPath);
}

int OverlayfsFuseFilesystemDriver::createDestinationPath(const util::fs::Path &containerDestinationPath)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = hostSource(util::fs::Path(mountPoint) / containerDestinationPath);
    if (!util::fs::createDirectories(host_destination_path, dest_mode)) {
        logErr() << "createDirectories" << host_destination_path << util::errnoString();
        return -1;
    }

    return 0;
}

int OverlayfsFuseFilesystemDriver::setup()
{
    // fork and exec fuse
    int pid = fork();
    if (0 == pid) {
        util::fs::createDirectories(util::fs::Path(workDir), 0755);
        util::fs::createDirectories(util::fs::Path(upperDir), 0755);
        util::fs::createDirectories(util::fs::Path(mountPoint), 0755);
        // fuse-overlayfs -o lowerdir=/ -o upperdir=./upper -o workdir=./work overlaydir
        util::strVec args;
        args.push_back("/usr/bin/fuse-overlayfs");
        args.push_back("-o");
        args.push_back("lowerdir=" + util::strVecJoin(lowerDirs, ':'));
        args.push_back("-o");
        args.push_back("upperdir=" + upperDir);
        args.push_back("-o");
        args.push_back("workdir=" + workDir);
        args.push_back(mountPoint);

        logErr() << util::exec(args, {});
        logErr() << util::errnoString();

        exit(0);
    }
    util::wait(pid);
    return 0;
}

util::fs::Path OverlayfsFuseFilesystemDriver::hostSource(const util::fs::Path &destFullPath) const
{
    return destFullPath;
}

util::fs::Path NativeFilesystemDriver::hostPath(const util::fs::Path &destFullPath) const
{
    return util::fs::Path(rootPath) / destFullPath;
}

util::fs::Path NativeFilesystemDriver::hostSource(const util::fs::Path &destFullPath) const
{
    return destFullPath;
}

int NativeFilesystemDriver::createDestinationPath(const util::fs::Path &containerDestinationPath)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = util::fs::Path(rootPath) / containerDestinationPath;

    util::fs::createDirectories(host_destination_path, dest_mode);

    return 0;
}

NativeFilesystemDriver::NativeFilesystemDriver(std::string rootPath)
    : rootPath(std::move(rootPath))
{
}

NativeFilesystemDriver::~NativeFilesystemDriver()
{
}

FuseProxyFilesystemDriver::FuseProxyFilesystemDriver(util::strVec mounts, std::string mount_point)
    : mounts(mounts)
    , mountPoint(mount_point)
{
}

util::fs::Path FuseProxyFilesystemDriver::hostPath(const util::fs::Path &destFullPath) const
{
    return hostSource(util::fs::Path(mountPoint) / destFullPath);
}

util::fs::Path FuseProxyFilesystemDriver::hostSource(const util::fs::Path &destFullPath) const
{
    return destFullPath;
}

int FuseProxyFilesystemDriver::createDestinationPath(const util::fs::Path &containerDestinationPath)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = hostSource(util::fs::Path(mountPoint) / containerDestinationPath);
    if (!util::fs::createDirectories(host_destination_path, dest_mode)) {
        logErr() << "createDirectories" << host_destination_path << util::errnoString();
        return -1;
    }

    return 0;
}

int FuseProxyFilesystemDriver::setup()
{
    int pipe_ends[2];
    if (0 != pipe(pipe_ends)) {
        logErr() << "pipe failed:" << util::errnoString();
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        logErr() << "fork failed:" << util::errnoString();
        return -1;
    }

    if (0 == pid) {
        close(pipe_ends[1]);
        if (-1 == dup2(pipe_ends[0], 112)) {
            logErr() << "dup2 failed:" << util::errnoString();
            return -1;
        }
        close(pipe_ends[0]);

        util::fs::createDirectories(util::fs::Path(mountPoint), 0755);
        util::fs::createDirectories(util::fs::Path(mountPoint + "/.root"), 0755);

        util::strVec args;
        args.push_back("/usr/bin/ll-fuse-proxy");
        args.push_back("112");
        args.push_back(mountPoint);

        logErr() << util::exec(args, {});

        exit(0);
    } else {
        close(pipe_ends[0]);
        std::string root_mount = mountPoint + "/.root:/\n";
        write(pipe_ends[1], root_mount.c_str(), root_mount.size()); // FIXME: handle write error
        for (auto const &m : mounts) {
            write(pipe_ends[1], m.c_str(), m.size());
        }
        close(pipe_ends[1]);
        return 0;
    }
}

} // namespace linglong
