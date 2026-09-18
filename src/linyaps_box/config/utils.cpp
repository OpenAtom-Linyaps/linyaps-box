// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/utils.h"

#include "linyaps_box/io/stream.h"
#include "linyaps_box/os/fs.h"

namespace linyaps_box::config {

auto read_json_to(const std::filesystem::path &path, utils::uninit_vector<std::byte> &buffer)
  -> std::size_t
{
    auto fd = os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only });
    if (UNLIKELY(!fd)) {
        throw std::filesystem::filesystem_error("failed to open config file", path, fd.error());
    }

    auto stat = os::fstat(*fd);
    if (UNLIKELY(!stat)) {
        throw std::system_error(stat.error(), "failed to stat config file");
    }

    auto type = os::to_fs_file_type(stat->st_mode);
    if (UNLIKELY(type != std::filesystem::file_type::regular)) {
        throw std::runtime_error("config file is not a regular file");
    }

    auto ret = io::read_to_end(fd.value(), buffer);
    if (UNLIKELY(!ret)) {
        throw std::system_error(ret.error(), "failed to read config file");
    }

    return *ret;
}

} // namespace linyaps_box::config
