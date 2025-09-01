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
    auto containers() -> std::unordered_map<std::string, container_ref>;

    auto create_container(const create_container_options_t &options) -> container;

private:
    std::unique_ptr<status_directory> status_dir_;
};

} // namespace linyaps_box
