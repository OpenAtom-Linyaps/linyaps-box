// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/runtime.h"

linyaps_box::runtime_t::runtime_t(std::unique_ptr<linyaps_box::status_directory> &&status_dir)
    : status_dir_{ std::move(status_dir) }
{
    if (!this->status_dir_) {
        throw std::invalid_argument("status_dir is nullptr");
    }
}

auto linyaps_box::runtime_t::containers()
        -> std::unordered_map<std::string, linyaps_box::container_ref>
{
    auto container_ids = this->status_dir_->list();

    std::unordered_map<std::string, container_ref> containers;
    for (const auto &container_id : container_ids) {
        containers.emplace(std::piecewise_construct,
                           std::forward_as_tuple(container_id),
                           std::forward_as_tuple(*this->status_dir_, container_id));
    }

    return containers;
}

auto linyaps_box::runtime_t::create_container(
        const linyaps_box::runtime_t::create_container_options_t &options) -> linyaps_box::container
{
    return { *this->status_dir_, options.ID, options.bundle, options.config, options.manager };
}
