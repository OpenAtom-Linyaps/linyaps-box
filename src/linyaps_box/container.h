// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/cgroup_manager.h"
#include "linyaps_box/container_ref.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box {

class container : public container_ref
{
public:
    container(std::shared_ptr<status_directory> status_dir,
              const std::string &id,
              const std::filesystem::path &bundle,
              std::filesystem::path config,
              cgroup_manager_t manager);

    [[nodiscard]] const linyaps_box::config &get_config() const;
    [[nodiscard]] const std::filesystem::path &get_bundle() const;
    [[nodiscard]] int run(const config::process_t &process);
    // TODO:: support fully container capabilities, e.g. create, start, stop, delete...
private:
    void cgroup_preenter(const cgroup_options &options, utils::file_descriptor &dirfd);
    std::filesystem::path bundle;
    linyaps_box::config config;
    std::unique_ptr<cgroup_manager> manager;
};

} // namespace linyaps_box
