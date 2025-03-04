// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <linux/magic.h>
#include <linyaps_box/utils/cgroups.h>
#include <sys/statfs.h>

#include <filesystem>
#include <system_error>

constexpr auto cgroup_root = "/sys/fs/cgroups";

linyaps_box::utils::cgroup_t linyaps_box::utils::get_cgroup_type()
{
    static auto cgroup_type = []() -> cgroup_t {
        struct statfs stat{};
        auto ret = statfs(cgroup_root, &stat);
        if (ret < 0) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    std::string{ "statfs " } + cgroup_root);
        }

        // for cgroup v2, filesystem type is cgroups2
        if (stat.f_type == CGROUP2_SUPER_MAGIC) {
            return cgroup_t::unified;
        }

        // for cgroup v1, filesystem type is tmpfs
        if (stat.f_type != TMPFS_MAGIC) {
            throw std::system_error(-1,
                                    std::system_category(),
                                    std::string{ "invalid file system type on " } + cgroup_root);
        }

        // https://man7.org/linux/man-pages/man7/cgroups.7.html
        auto unified = std::filesystem::path{ cgroup_root } / "unified";
        ret = statfs(unified.c_str(), &stat);
        if (ret < 0) {
            if (errno != ENOENT) {
                throw std::system_error(errno,
                                        std::system_category(),
                                        "statfs " + unified.string());
            }

            return cgroup_t::legacy;
        }

        return stat.f_type == CGROUP2_SUPER_MAGIC ? cgroup_t::hybrid : cgroup_t::legacy;
    }();

    return cgroup_type;
}
