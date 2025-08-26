// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/impl/disabled_cgroup_manager.h"

namespace linyaps_box {
[[nodiscard]] auto disabled_cgroup_manager::type() const -> cgroup_manager_t
{
    return cgroup_manager_t::disabled;
}

auto disabled_cgroup_manager::create_cgroup([[maybe_unused]] const cgroup_options &options)
        -> cgroup_status
{
    cgroup_status status{};
    set_manager(status, type());
    return status;
}

void disabled_cgroup_manager::precreate_cgroup([[maybe_unused]] const cgroup_options &options,
                                               [[maybe_unused]] utils::file_descriptor &dirfd)
{
}

void disabled_cgroup_manager::destroy_cgroup([[maybe_unused]] const cgroup_status &status) { }
} // namespace linyaps_box
