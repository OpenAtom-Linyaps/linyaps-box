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

        uint32_t flags = m.flags;
        util::str_vec options;

        for (const auto &option : m.options) {
            if ("bind" == option) {
                flags |= MS_BIND;
                continue;
            }
            options.push_back(option);

            // FIXME(iceyer): it that work???
            if (option.rfind("mode=", 0) == 0) {
                auto equalPos = option.find('=');
                auto mode = option.substr(equalPos + 1, option.length() - equalPos - 1);
                // FIXME(iceyer): should change here or in mount
                chmod(dest_full_path.string().c_str(), std::stoi(mode, nullptr, 8));
            }
        }

        auto opts = util::str_vec_join(options, ',');

        switch (m.fsType) {
        case Mount::Bind:
            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), nullptr, MS_BIND | MS_REC, opts.c_str());
            if (0 != ret) {
                break;
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
            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), m.type.c_str(), flags, nullptr);
            break;
        case Mount::Cgroup:
            ret = ::mount(source.c_str(), host_dest_full_path.string().c_str(), m.type.c_str(), flags, opts.c_str());
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

    std::unique_ptr<FilesystemDriver> driver_;
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

HostMount::~HostMount()
{
};

} // namespace linglong