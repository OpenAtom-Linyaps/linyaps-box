// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <linyaps_box/cgroup.h>
#include <linyaps_box/interface.h>

namespace linyaps_box {
class cgroup_manager : public virtual interface
{
public:
    [[nodiscard]] virtual cgroup_manager_t type() const = 0;

    virtual cgroup_status create_cgroup(const cgroup_options &options) = 0;

    virtual void precreate_cgroup(const cgroup_options &options, utils::file_descriptor &dirfd) = 0;

    virtual void destroy_cgroup(const cgroup_status &status) = 0;

    // TODO: support update resource
    // virtual void update_resource(const cgroup_status &status, ) = 0;
protected:
    static void set_manager_type(cgroup_status &status, cgroup_manager_t type) noexcept
    {
        status.manager = type;
    }
};

} // namespace linyaps_box
