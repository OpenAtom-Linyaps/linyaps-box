/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_
#define LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_

#include <vector>
#include <ostream>
#include <system_error>

#include "common.h"

namespace linglong {
namespace util {
namespace fs {

class path : public std::basic_string<char>
{
public:
    explicit path(const std::string &s)
        : p(util::str_spilt(s, "/"))
    {
    }

    path &operator=(const std::string &s)
    {
        p = util::str_spilt(s, "/");
        return *this;
    }

    path &operator=(const path &p1) = default;

    bool operator==(const path &s) const { return this->p == s.p; }

    bool operator!=(const path &s) const { return this->p != s.p; }

    path operator/(const path &p1) const
    {
        auto np = *this;
        std::copy(p1.p.begin(), p1.p.end(), back_inserter(np.p));
        return np;
    }

    path operator/(const std::string &str) const { return operator/(path(str)); }

    path parent_path() const
    {
        path pn(*this);
        pn.p.pop_back();
        return pn;
    }

    std::string string() const { return "/" + str_vec_join(p, '/'); }

    str_vec components() const { return p; }

private:
    friend std::ostream &operator<<(std::ostream &cout, path obj);
    std::vector<std::string> p;
};

inline std::ostream &operator<<(std::ostream &cout, path obj)
{
    for (auto const &s : obj.p) {
        cout << "/" << s;
    }
    return cout;
}

bool create_directories(const path &p, __mode_t mode);

enum file_type {
    status_error,
    file_not_found,
    regular_file,
    directory_file,
    symlink_file,
    block_file,
    character_file,
    fifo_file,
    socket_file,
    reparse_file,
    type_unknown
};

enum perms {
    no_perms,
    owner_read,
    owner_write,
    owner_exe,
    owner_all,
    group_read,
    group_write,
    group_exe,
    group_all,
    others_read,
    others_write,
    others_exe,
    others_all,
    all_all,
    set_uid_on_exe,
    set_gid_on_exe,
    sticky_bit,
    perms_mask,
    perms_not_known,
    add_perms,
    remove_perms,
    symlink_perms
};

class file_status
{
public:
    // constructors
    file_status() noexcept;
    explicit file_status(file_type ft, perms p = perms_not_known) noexcept;

    // compiler generated
    file_status(const file_status &) noexcept;
    file_status &operator=(const file_status &) noexcept;
    ~file_status() noexcept;

    // observers
    file_type type() const noexcept;
    perms permissions() const noexcept;

private:
    file_type ft;
    perms p;
};

bool is_dir(const std::string &s);

bool exists(const std::string &s);

file_status status(const path &p, std::error_code &ec);

path read_symlink(const path &p);

} // namespace fs
} // namespace util
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_FILESYSTEM_H_ */
