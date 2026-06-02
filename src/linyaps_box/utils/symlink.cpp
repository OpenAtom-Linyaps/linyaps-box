// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/symlink.h"

#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/utils/utils.h"

#include <linux/limits.h>

void linyaps_box::utils::symlink(const std::filesystem::path &target,
                                 const std::filesystem::path &link_path)
{
    LINYAPS_BOX_DEBUG() << "Create symlink " << link_path << " point to " << target;

    std::error_code ec;
    std::filesystem::create_symlink(target, link_path, ec);
    if (ec) {
        throw std::system_error{ ec.value(), std::system_category(), ec.message() };
    }
}

void linyaps_box::utils::symlink_at(const std::filesystem::path &target,
                                    const file_descriptor &dirfd,
                                    const std::filesystem::path &link_path)
{
    LINYAPS_BOX_DEBUG() << "Create symlink " << link_path << " which under " << dirfd.current_path()
                        << " point to " << target;

    const auto ret = ::symlinkat(target.c_str(), dirfd.get(), link_path.c_str());
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "symlinkat");
    }
}

void linyaps_box::utils::symlink_at(const std::filesystem::path &target,
                                    const file_descriptor &dirfd,
                                    const std::filesystem::path &link_path,
                                    std::error_code &ec) noexcept
{
    LINYAPS_BOX_DEBUG() << "Create symlink " << link_path << " which under " << dirfd.current_path()
                        << " point to " << target;

    ec.clear();
    const auto ret = ::symlinkat(target.c_str(), dirfd.get(), link_path.c_str());
    if (ret == -1) {
        ec.assign(errno, std::system_category());
    }
}

std::filesystem::path linyaps_box::utils::readlink(const std::filesystem::path &path)
{
    std::error_code ec;
    auto ret = std::filesystem::read_symlink(path, ec);
    if (ec) {
        throw std::system_error{ ec.value(), std::system_category(), ec.message() };
    }

    return ret;
}

std::filesystem::path linyaps_box::utils::readlinkat(const file_descriptor &dirfd,
                                                     const std::filesystem::path &path)
{
    auto buf_len = get_path_max(dirfd.current_path()) + 1;
    std::string buf(buf_len, '\0');
    auto ret = ::readlinkat(dirfd.get(), path.c_str(), buf.data(), buf_len - 1);
    if (UNLIKELY(ret == -1)) {
        throw std::system_error(errno, std::system_category(), "readlinkat");
    }

    buf.resize(static_cast<size_t>(ret));
    return std::filesystem::path{ std::move(buf) };
}
