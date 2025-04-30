// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container.h"
#include "linyaps_box/container_ref.h"
#include "linyaps_box/status_directory.h"

#include <memory>
#include <unordered_map>

namespace linyaps_box {

class runtime_t
{
public:
    explicit runtime_t(std::unique_ptr<status_directory> &&status_dir);
    std::unordered_map<std::string, container_ref> containers();

    struct create_container_options_t
    {
        std::filesystem::path bundle;
        std::filesystem::path config;
        std::string ID;
        cgroup_manager_t manager;
    };

    container create_container(const create_container_options_t &options);

private:
    std::unique_ptr<status_directory> status_dir;
};

} // namespace linyaps_box
