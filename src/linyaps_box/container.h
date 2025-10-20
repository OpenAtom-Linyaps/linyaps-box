// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/cgroup_manager.h"
#include "linyaps_box/container_ref.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box {

struct container_data;

struct create_container_options_t
{
    cgroup_manager_t manager;
    int preserve_fds;
    std::string ID;
    std::filesystem::path bundle;
    std::filesystem::path config;
};

class container final : public container_ref
{
public:
    container(const status_directory &status_dir, const create_container_options_t &options);

    container(const container &) = delete;
    auto operator=(const container &) -> container & = delete;
    container(container &&) = delete;
    auto operator=(container &&) -> container & = delete;

    [[nodiscard]] auto get_config() const -> const linyaps_box::config &;
    [[nodiscard]] auto get_bundle() const -> const std::filesystem::path &;
    [[nodiscard]] auto run(const config::process_t &process) const -> int;
    // TODO:: support fully container capabilities, e.g. create, start, stop, delete...
    friend auto get_private_data(const container &c) noexcept -> container_data &;
    ~container() noexcept override;

    [[nodiscard]] auto host_gid() const noexcept { return host_gid_; }

    [[nodiscard]] auto host_uid() const noexcept { return host_uid_; }

    [[nodiscard]] auto preserve_fds() const noexcept { return preserve_fds_; }

private:
    void cgroup_preenter(const cgroup_options &options, utils::file_descriptor &dirfd);
    int preserve_fds_;
    gid_t host_gid_;
    uid_t host_uid_;
    container_data *data{ nullptr };
    std::filesystem::path bundle;
    std::unique_ptr<cgroup_manager> manager;
    linyaps_box::config config;
};

} // namespace linyaps_box
