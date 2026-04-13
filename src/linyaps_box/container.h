// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/cgroup_manager.h"
#include "linyaps_box/container_ref.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/unix_socket.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box {

struct container_data;

struct create_container_options_t
{
    cgroup_manager_t manager;
    std::string ID;
    std::filesystem::path bundle;
    std::filesystem::path config;
};

struct run_container_options_t
{
    int preserve_fds;
    std::optional<unix_socket> console_socket;
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
    [[nodiscard]] auto run(run_container_options_t options) -> int;

    // TODO:: support fully container capabilities, e.g. create, start, stop, delete...

    ~container() noexcept override = default;

    [[nodiscard]] auto host_gid() const noexcept { return host_gid_; }

    [[nodiscard]] auto host_uid() const noexcept { return host_uid_; }

    [[nodiscard]] auto deny_setgroups() const noexcept { return deny_setgroups_; }

    [[nodiscard]] auto mount_dev_from_host() const noexcept { return mount_dev_from_host_; }

    [[nodiscard]] auto rootfs_propagation() const noexcept { return rootfs_propagation_; }

    auto set_rootfs_propagation(unsigned int propagation) noexcept
    {
        rootfs_propagation_ = propagation;
    }

    auto set_deny_setgroups() noexcept { deny_setgroups_ = true; }

    auto set_mount_dev_from_host() noexcept { mount_dev_from_host_ = true; }

private:
    void cgroup_preenter(const cgroup_options &options, utils::file_descriptor &dirfd);
    linyaps_box::config config;
    std::filesystem::path bundle;
    std::unique_ptr<cgroup_manager> manager;
    unsigned int rootfs_propagation_{ 0 };
    gid_t host_gid_;
    uid_t host_uid_;
    bool deny_setgroups_{ false };
    bool mount_dev_from_host_{ false };
};

} // namespace linyaps_box
