/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>
#include <climits>
#include <unistd.h>

#include "filesystem.h"
#include "logger.h"

namespace linglong {
namespace util {
namespace fs {

using namespace std;

bool create_directory(const Path &path, __mode_t mode)
{
    return mkdir(path.string().c_str(), mode);
}

bool createDirectories(const Path &path, __mode_t mode)
{
    std::string fullPath;
    for (const auto &e : path.components()) {
        fullPath += "/" + e;
        if (isDir(fullPath)) {
            continue;
        }

        auto ret = mkdir(fullPath.c_str(), mode);
        if (0 != ret) {
            logErr() << util::retErrString(ret) << fullPath << mode;
            return false;
        }
    }
    return true;
}

bool isDir(const std::string &str)
{
    struct stat st {
    };

    if (0 != lstat(str.c_str(), &st)) {
        return false;
    }

    switch (st.st_mode & S_IFMT) {
    case S_IFDIR:
        return true;
    default:
        return false;
    }
}

bool exists(const std::string &s)
{
    struct stat st {
    };

    if (0 != lstat(s.c_str(), &st)) {
        return false;
    }
    return true;
}

Path readSymlink(const Path &path)
{
    char *buf = realpath(path.string().c_str(), nullptr);
    if (buf) {
        auto ret = Path(string(buf));
        free(buf);
        return ret;
    } else {
        return path;
    }
}

FileStatus status(const Path &path, std::error_code &errorCode)
{
    FileType fileType;
    Perms perm = kNoPerms;

    struct stat st {
    };

    if (0 != lstat(path.string().c_str(), &st)) {
        if (errno == ENOENT) {
            fileType = kFileNotFound;
        } else {
            fileType = kStatusError;
        }
        return FileStatus(fileType, perm);
    }

    // FIXME: Perms
    // https://www.boost.org/doc/libs/1_75_0/libs/filesystem/doc/reference.html#FileStatus
    //    int st_perm = st.st_mode & 0xFFFF;

    //    switch (st_perm) {
    //    case S_IRUSR:
    //        perm = kOwnerRead;
    //        break;
    //    case S_IWUSR:
    //    case S_IXUSR:
    //    case S_IRWXU:
    //    case S_IRGRP:
    //    }

    switch (st.st_mode & S_IFMT) {
    case S_IFREG:
        fileType = kRegularFile;
        break;
    case S_IFDIR:
        fileType = kDirectoryFile;
        break;
    case S_IFLNK:
        fileType = kSymlinkFile;
        break;
    case S_IFBLK:
        fileType = kBlockFile;
        break;
    case S_IFCHR:
        fileType = kCharacterFile;
        break;
    case S_IFIFO:
        fileType = kFifoFile;
    case S_IFSOCK:
        break;
    default:
        fileType = kTypeUnknown;
        break;
    }

    return FileStatus(fileType, perm);
}

FileStatus::FileStatus() noexcept
{
}

FileStatus::FileStatus(FileType fileType, Perms perms) noexcept
    : fileType(fileType)
    , perms(perms)
{
}

FileStatus::FileStatus(const FileStatus &fs) noexcept
{
    fileType = fs.fileType;
    perms = fs.perms;
}

FileStatus &FileStatus::operator=(const FileStatus &fs) noexcept
{
    fileType = fs.fileType;
    perms = fs.perms;
    return *this;
}

FileStatus::~FileStatus() noexcept = default;

FileType FileStatus::type() const noexcept
{
    return fileType;
}

Perms FileStatus::permissions() const noexcept
{
    return perms;
}

int doMountWithFd(const char *root, const char *__special_file, const char *__dir, const char *__fstype,
                  unsigned long int __rwflag, const void *__data) __THROW
{
    // https://github.com/opencontainers/runc/blob/0ca91f44f1664da834bc61115a849b56d22f595f/libcontainer/utils/utils.go#L112

    int fd = open(__dir, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        logFal() << util::format("fail to open target(%s):", __dir) << errnoString();
    }

    // Refer to `man readlink`, readlink dose not append '\0' to the end of conent it read from Path, so we have to add
    // an extra char to buffer to ensure '\0' always exists.
    char *buf = (char *)malloc(sizeof(char) * PATH_MAX + 1);
    if (buf == nullptr) {
        logFal() << "fail to alloc memery:" << errnoString();
    }

    memset(buf, 0, PATH_MAX + 1);

    auto target = util::format("/proc/self/fd/%d", fd);
    int len = readlink(target.c_str(), buf, PATH_MAX);
    if (len == -1) {
        logFal() << util::format("fail to readlink from proc fd (%s):", target.c_str()) << errnoString();
    }

    string realpath(buf);
    free(buf);
    if (realpath.rfind(root, 0) != 0) {
        logDbg() << util::format("container root=\"%s\"", root);
        logFal() << util::format("possibly malicious Path detected (%s vs %s) -- refusing to operate", target.c_str(),
                                 realpath.c_str());
    }

    auto ret = ::mount(__special_file, target.c_str(), __fstype, __rwflag, __data);
    auto oldErrNo = errno;

    close(fd);

    errno = oldErrNo;
    return ret;
}

} // namespace fs
} // namespace util
} // namespace linglong
