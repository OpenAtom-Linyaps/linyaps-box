/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/wait.h>

#include <utility>

#include "filesystem_driver.h"
#include "util/platform.h"

namespace linglong {

OverlayfsFuseFilesystemDriver::OverlayfsFuseFilesystemDriver(util::str_vec lower_dirs, std::string upper_dir,
                                                             std::string work_dir, std::string mount_point)
    : lower_dirs_(std::move(lower_dirs))
    , upper_dir_(std::move(upper_dir))
    , work_dir_(std::move(work_dir))
    , mount_point_(std::move(mount_point))
{
}

util::fs::path OverlayfsFuseFilesystemDriver::HostPath(const util::fs::path &dest_full_path) const
{
    return HostSource(util::fs::path(mount_point_) / dest_full_path);
}

int OverlayfsFuseFilesystemDriver::CreateDestinationPath(const util::fs::path &container_destination_path)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = HostSource(util::fs::path(mount_point_) / container_destination_path);
    if (!util::fs::create_directories(host_destination_path, dest_mode)) {
        logErr() << "create_directories" << host_destination_path << util::errnoString();
        return -1;
    }

    return 0;
}

int OverlayfsFuseFilesystemDriver::Setup()
{
    // fork and exec fuse
    int pid = fork();
    if (0 == pid) {
        util::fs::create_directories(util::fs::path(work_dir_), 0755);
        util::fs::create_directories(util::fs::path(upper_dir_), 0755);
        util::fs::create_directories(util::fs::path(mount_point_), 0755);
        // fuse-overlayfs -o lowerdir=/ -o upperdir=./upper -o workdir=./work overlaydir
        util::str_vec args;
        args.push_back("/usr/bin/fuse-overlayfs");
        args.push_back("-o");
        args.push_back("lowerdir=" + util::str_vec_join(lower_dirs_, ':'));
        args.push_back("-o");
        args.push_back("upperdir=" + upper_dir_);
        args.push_back("-o");
        args.push_back("workdir=" + work_dir_);
        args.push_back(mount_point_);

        logErr() << util::Exec(args, {});
        logErr() << util::errnoString();

        exit(0);
    }

    return waitpid(pid, 0, 0);
}

util::fs::path OverlayfsFuseFilesystemDriver::HostSource(const util::fs::path &dest_full_path) const
{
    return dest_full_path;
}

util::fs::path NativeFilesystemDriver::HostPath(const util::fs::path &dest_full_path) const
{
    return util::fs::path(root_path_) / dest_full_path;
}

util::fs::path NativeFilesystemDriver::HostSource(const util::fs::path &dest_full_path) const
{
    return dest_full_path;
}

int NativeFilesystemDriver::CreateDestinationPath(const util::fs::path &container_destination_path)
{
    __mode_t dest_mode = 0755;

    auto host_destination_path = util::fs::path(root_path_) / container_destination_path;

    util::fs::create_directories(host_destination_path, dest_mode);

    return 0;
}

NativeFilesystemDriver::NativeFilesystemDriver(std::string root_path)
    : root_path_(std::move(root_path))
{
}

NativeFilesystemDriver::~NativeFilesystemDriver()
{
}

} // namespace linglong