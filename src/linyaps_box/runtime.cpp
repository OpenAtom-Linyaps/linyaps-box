// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/runtime.h"

linyaps_box::runtime_t::runtime_t(status_directory_manager status_dir_mgr)
    : status_dir_mgr_(std::move(status_dir_mgr))
{
}

auto linyaps_box::runtime_t::containers()
  -> std::unordered_map<std::string, linyaps_box::container_ref>
{
    auto container_ids = status_dir_mgr_.list();

    std::unordered_map<std::string, container_ref> containers;
    for (const auto &container_id : container_ids) {
        containers.emplace(std::piecewise_construct,
                           std::forward_as_tuple(container_id),
                           std::forward_as_tuple(status_dir_mgr_.get(container_id), container_id));
    }

    return containers;
}

auto linyaps_box::runtime_t::create_container(const create_container_options_t &options)
  -> linyaps_box::container
{
    return { status_dir_mgr_.get(options.ID), options };
}
