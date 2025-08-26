// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <linyaps_box/cgroup_manager.h>

namespace linyaps_box {

class disabled_cgroup_manager : public virtual cgroup_manager
{
public:
    [[nodiscard]] auto type() const -> cgroup_manager_t override;

    auto create_cgroup([[maybe_unused]] const cgroup_options &options) -> cgroup_status override;

    void precreate_cgroup([[maybe_unused]] const cgroup_options &options,
                          [[maybe_unused]] utils::file_descriptor &dirfd) override;

    void destroy_cgroup([[maybe_unused]] const cgroup_status &status) override;
};

} // namespace linyaps_box
