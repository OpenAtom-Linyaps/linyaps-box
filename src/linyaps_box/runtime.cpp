// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/runtime.h"

linyaps_box::runtime_t::runtime_t(std::unique_ptr<linyaps_box::status_directory> &&status_dir)
    : status_dir{ std::move(status_dir) }
{
    if (!this->status_dir) {
        throw std::invalid_argument("status_dir is nullptr");
    }
}

std::unordered_map<std::string, linyaps_box::container_ref> linyaps_box::runtime_t::containers()
{
    auto container_ids = this->status_dir->list();

    std::unordered_map<std::string, container_ref> containers;
    for (const auto &container_id : container_ids) {
        containers.emplace(container_id, container_ref(this->status_dir, container_id));
    }

    return containers;
}

linyaps_box::container linyaps_box::runtime_t::create_container(
        const linyaps_box::runtime_t::create_container_options_t &options)
{
    return { this->status_dir, options.ID, options.bundle, options.config, options.manager };
}
