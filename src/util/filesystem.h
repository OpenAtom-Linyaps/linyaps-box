/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_
#define LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_

#include <vector>
#include <ostream>
#include <system_error>

#include <unistd.h>

#include "common.h"

namespace linglong {
namespace util {
namespace fs {

class Path : public std::basic_string<char>
{
public:
    explicit Path(const std::string &path)
        : strVector(util::strSpilt(path, "/"))
    {
    }

    Path &operator=(const std::string &path)
    {
        strVector = util::strSpilt(path, "/");
        return *this;
    }

    Path &operator=(const Path &path) = default;

    bool operator==(const Path &path) const { return this->strVector == path.strVector; }

    bool operator!=(const Path &path) const { return this->strVector != path.strVector; }

    Path operator/(const Path &path) const
    {
        auto np = *this;
        std::copy(path.strVector.begin(), path.strVector.end(), back_inserter(np.strVector));
        return np;
    }

    Path operator/(const std::string &str) const { return operator/(Path(str)); }

    Path parent_path() const
    {
        Path pn(*this);
        pn.strVector.pop_back();
        return pn;
    }

    std::string string() const { return "/" + strVecJoin(strVector, '/'); }

    strVec components() const { return strVector; }

    // call to this function will block until `Path` exists (rerturn 0) or timeout (return -1)
    // default timeout is 1 second
    int waitUntilExsit(int utimeout = 1000)
    {
        int time = 0;
        while (true) {
            if (access(this->string().c_str(), F_OK) == 0) {
                return 0;
            } else if (++time > utimeout) {
                return -1;
            } else {
                usleep(10);
            }
        }
        return 0;
    }

private:
    friend std::ostream &operator<<(std::ostream &cout, Path path);
    std::vector<std::string> strVector;
};

inline std::ostream &operator<<(std::ostream &cout, Path path)
{
    for (auto const &str : path.strVector) {
        cout << "/" << str;
    }
    return cout;
}

bool createDirectories(const Path &path, __mode_t mode);

enum FileType {
    kStatusError,
    kFileNotFound,
    kRegularFile,
    kDirectoryFile,
    kSymlinkFile,
    kBlockFile,
    kCharacterFile,
    kFifoFile,
    kSocketFile,
    kReparseFile,
    kTypeUnknown
};

enum Perms {
    kNoPerms,
    kOwnerRead,
    kOwnerWrite,
    kOwnerExe,
    kOwnerAll,
    kGroupRead,
    kGroupWrite,
    kGroupExe,
    kGroupAll,
    kOthersRead,
    kOthersWrite,
    kOthersExe,
    kOthersAll,
    kAllAll,
    kSetUidOnExe,
    kSetGidOnExe,
    kStickyBit,
    PermsMask,
    kPermsNotKnown,
    AddPerms,
    kRemovePerms,
    kSymlinkPerms
};

class FileStatus
{
public:
    // constructors
    FileStatus() noexcept;
    explicit FileStatus(FileType fileType, Perms perms = kPermsNotKnown) noexcept;

    // compiler generated
    FileStatus(const FileStatus &) noexcept;
    FileStatus &operator=(const FileStatus &) noexcept;
    ~FileStatus() noexcept;

    // observers
    FileType type() const noexcept;
    Perms permissions() const noexcept;

private:
    FileType fileType;
    Perms perms;
};

bool isDir(const std::string &str);

bool exists(const std::string &str);

FileStatus status(const Path &strVector, std::error_code &errorCode);

Path readSymlink(const Path &strVector);

// This function doMountWithFd do mount in a secure way by check the target we are going to mount is within container
// rootfs or not before actually call mount.
// refer to
// https://github.com/opencontainers/runc/commit/0ca91f44f1664da834bc61115a849b56d22f595f
// NOTE: we should never directly do mount exists in oci json without this check.
int doMountWithFd(const char *root, const char *__special_file, const char *__dir, const char *__fstype,
                  unsigned long int __rwflag, const void *__data) __THROW;

} // namespace fs
} // namespace util
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_ */
