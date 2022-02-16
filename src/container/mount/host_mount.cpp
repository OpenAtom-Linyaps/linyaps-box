/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

    int CreateDestinationPath(const util::fs::path &container_destination_path) const
    {
        return driver_->CreateDestinationPath(container_destination_path);
    }

    int MountNode(const struct Mount &m) const
    {
        int ret = -1;
        struct stat source_stat {
        };
        bool is_path = false;

        auto source = m.source;

        if (!m.source.empty() && m.source[0] == '/') {
            is_path = true;
            source = driver_->HostSource(util::fs::path(m.source)).string();
        }

        ret = lstat(source.c_str(), &source_stat);
        if (0 == ret) {
        } else {
            // source not exist
            if (m.fsType == Mount::Bind) {
                logErr() << "lstat" << source << "failed";
                return -1;
            }
        }

        auto dest_full_path = util::fs::path(m.destination);
        auto dest_parent_path = util::fs::path(dest_full_path).parent_path();
        auto host_dest_full_path = driver_->HostPath(dest_full_path);

        switch (source_stat.st_mode & S_IFMT) {
        case S_IFCHR: {
            driver_->CreateDestinationPath(dest_parent_path);
            std::ofstream output(host_dest_full_path.string());
            break;
        }
        case S_IFSOCK: {
            driver_->CreateDestinationPath(dest_parent_path);
            // FIXME: can not mound dbus socket on rootless
            std::ofstream output(host_dest_full_path.string());
            break;
        }
        case S_IFLNK: {
            driver_->CreateDestinationPath(dest_parent_path);
            std::ofstream output(host_dest_full_path.string());
            source = util::fs::read_symlink(util::fs::path(source)).string();
            break;
        }
        case S_IFREG: {
            driver_->CreateDestinationPath(dest_parent_path);
            std::ofstream output(host_dest_full_path.string());
            break;
        }
        case S_IFDIR:
            driver_->CreateDestinationPath(dest_full_path);
            break;
        default:
            driver_->CreateDestinationPath(dest_full_path);
            if (is_path) {
                logWan() << "unknown file type" << (source_stat.st_mode & S_IFMT) << source;
            }
            break;
        }

        auto opt_pair = praseMountOptions(m);
        auto flags = opt_pair.first;
        auto opts = opt_pair.second;

        switch (m.fsType) {
        case Mount::Bind:
            ret =
                ::mount(source.c_str(), host_dest_full_path.string().c_str(), nullptr, MS_BIND | MS_REC, opts.c_str());
            if (0 != ret) {
                break;
            }

            if (source == "/sys") {
                sysfs_is_binded = true;
            }

            if (opts.empty()) {
                break;
            }

            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), nullptr, flags | MS_BIND | MS_REMOUNT,
                          opts.c_str());
            if (0 != ret) {
                break;
            }
            break;
        case Mount::Proc:
        case Mount::Devpts:
        case Mount::Mqueue:
        case Mount::Tmpfs:
        case Mount::Sysfs:
            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), m.type.c_str(), flags, opts.c_str());
            if (ret < 0) {
                // refers:
                // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L178
                // https://github.com/containers/crun/blob/38e1b5e2a3e9567ff188258b435085e329aaba42/src/libcrun/linux.c#L768-L789
                if (m.fsType == Mount::Sysfs) {
                    ret = ::mount("/sys", host_dest_full_path.string().c_str(), nullptr, MS_BIND | MS_REC, nullptr);
                    if (ret == 0) {
                        sysfs_is_binded = true;
                    }
                } else if (m.fsType == Mount::Mqueue) {
                    ret = ::mount("/dev/mqueue", host_dest_full_path.string().c_str(), nullptr, MS_BIND | MS_REC,
                                  nullptr);
                }
            }
            break;
        case Mount::Cgroup:
            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), m.type.c_str(), flags, opts.c_str());
            // When sysfs is bind-mounted, It is ok to let cgroup mount failed.
            // https://github.com/containers/podman/blob/466b8991c4025006eeb43cb30e6dc990d92df72d/pkg/specgen/generate/oci.go#L281
            if (sysfs_is_binded) {
                ret = 0;
            }
            break;
        default:
            logErr() << "unsupported type" << m.type;
        }

        if (EXIT_SUCCESS != ret) {
            logErr() << "mount" << source << "to" << host_dest_full_path << "failed:" << util::RetErrString(ret)
                     << "\nmount args is:" << m.type << flags << opts;
            if (is_path) {
                logErr() << "source file type is: 0x" << std::hex << (source_stat.st_mode & S_IFMT);
                DUMP_FILE_INFO(source);
            }
            DUMP_FILE_INFO(host_dest_full_path.string());
        }

        return ret;
    }

    struct MountFlag {
        bool clear;
        uint32_t flag;
    };

    // parses options and returns a flag and data depends on options set accordingly.
    std::pair<uint32_t, std::string> praseMountOptions(const struct Mount &m) const
    {
        // TODO support "propagation flags" and "recursive mount attrs"
        // https://github.com/opencontainers/runc/blob/c83abc503de7e8b3017276e92e7510064eee02a8/libcontainer/specconv/spec_linux.go#L958
        uint32_t flags = m.flags;
        util::str_vec options;

        for (const auto &option : m.options) {
            auto it = mount_flags.find(option);
            if (it != mount_flags.end()) {
                auto mountFlag = it->second;
                if (mountFlag.clear)
                    flags &= ~mountFlag.flag;
                else
                    flags |= mountFlag.flag;
            } else {
                options.push_back(option);
            }
        }

        auto data = util::str_vec_join(options, ',');
        return std::make_pair(flags, data);
    }

    std::unique_ptr<FilesystemDriver> driver_;
    static std::map<std::string, MountFlag> mount_flags;
    mutable bool sysfs_is_binded = false;
};

std::map<std::string, HostMountPrivate::MountFlag> HostMountPrivate::mount_flags = {
    {"acl", {false, MS_POSIXACL}},
    {"async", {true, MS_SYNCHRONOUS}},
    {"atime", {true, MS_NOATIME}},
    {"bind", {false, MS_BIND}},
    {"defaults", {false, 0}},
    {"dev", {true, MS_NODEV}},
    {"diratime", {true, MS_NODIRATIME}},
    {"dirsync", {false, MS_DIRSYNC}},
    {"exec", {true, MS_NOEXEC}},
    {"iversion", {false, MS_I_VERSION}},
    {"lazytime", {false, MS_LAZYTIME}},
    {"loud", {true, MS_SILENT}},
    {"mand", {false, MS_MANDLOCK}},
    {"noacl", {true, MS_POSIXACL}},
    {"noatime", {false, MS_NOATIME}},
    {"nodev", {false, MS_NODEV}},
    {"nodiratime", {false, MS_NODIRATIME}},
    {"noexec", {false, MS_NOEXEC}},
    {"noiversion", {true, MS_I_VERSION}},
    {"nolazytime", {true, MS_LAZYTIME}},
    {"nomand", {true, MS_MANDLOCK}},
    {"norelatime", {true, MS_RELATIME}},
    {"nostrictatime", {true, MS_STRICTATIME}},
    {"nosuid", {false, MS_NOSUID}},
    // {"nosymfollow",{false, MS_NOSYMFOLLOW}}, // since kernel 5.10
    {"rbind", {false, MS_BIND | MS_REC}},
    {"relatime", {false, MS_RELATIME}},
    {"remount", {false, MS_REMOUNT}},
    {"ro", {false, MS_RDONLY}},
    {"rw", {true, MS_RDONLY}},
    {"silent", {false, MS_SILENT}},
    {"strictatime", {false, MS_STRICTATIME}},
    {"suid", {true, MS_NOSUID}},
    {"sync", {false, MS_SYNCHRONOUS}},
    // {"symfollow",{true, MS_NOSYMFOLLOW}}, // since kernel 5.10
};

HostMount::HostMount()
    : dd_ptr(new HostMountPrivate())
{
}

int HostMount::MountNode(const struct Mount &m)
{
    return dd_ptr->MountNode(m);
}

int HostMount::Setup(FilesystemDriver *driver)
{
    if (nullptr == driver) {
        logWan() << this << dd_ptr->driver_.get();
        return 0;
    }

    dd_ptr->driver_ = std::unique_ptr<FilesystemDriver>(driver);
    return dd_ptr->driver_->Setup();
}

HostMount::~HostMount() {};

} // namespace linglong
