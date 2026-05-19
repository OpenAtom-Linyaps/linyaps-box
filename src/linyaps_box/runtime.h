// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container.h"
#include "linyaps_box/container_ref.h"
#include "linyaps_box/status_directory_manager.h"

#include <string>
#include <unordered_map>

namespace linyaps_box {

class runtime_t
{
public:
    explicit runtime_t(status_directory_manager status_dir_mgr);
    auto containers() -> std::unordered_map<std::string, container_ref>;

    auto create_container(const create_container_options_t &options) -> container;

private:
    status_directory_manager status_dir_mgr_;
};

} // namespace linyaps_box
