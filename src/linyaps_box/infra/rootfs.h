// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <filesystem>
#include <variant>

#include <sys/types.h>

namespace linyaps_box::infra {

struct symlink_spec
{
    std::filesystem::path target;
};

struct hardlink_spec
{
    std::filesystem::path target;
};

struct fifo_spec
{
    std::filesystem::perms mode;
};

struct character_device_spec
{
    std::filesystem::perms mode;
    dev_t dev;
};

struct block_device_spec
{
    std::filesystem::perms mode;
    dev_t dev;
};

using inode_type =
  std::variant<symlink_spec, hardlink_spec, fifo_spec, character_device_spec, block_device_spec>;

class Root
{
public:
    Root() noexcept = default;

    Root(const Root &) = delete;
    auto operator=(const Root &) -> Root & = delete;
    Root(Root &&) noexcept = default;
    auto operator=(Root &&) noexcept -> Root & = default;
    ~Root() noexcept = default;

    [[nodiscard]] static auto open(const std::filesystem::path &rootfs) noexcept
      -> os::Result<Root>;

    [[nodiscard]] auto reopen() noexcept -> os::Result<void>;

    [[nodiscard]] auto valid() const noexcept -> bool;

    [[nodiscard]] auto fd() const noexcept -> int;

    [[nodiscard]] auto ref() const noexcept -> utils::file_descriptor_ref;

    [[nodiscard]] auto open(const std::filesystem::path &path,
                            os::sys::open_option opt,
                            std::filesystem::perms perm = os::default_file_perm) const noexcept
      -> os::Result<utils::file_descriptor>;

    [[nodiscard]] auto
    create_directory(const std::filesystem::path &path,
                     std::filesystem::perms perm = os::default_dir_perm) const noexcept
      -> os::Result<utils::file_descriptor>;

    [[nodiscard]] auto
    create_directories(const std::filesystem::path &path,
                       std::filesystem::perms perm = os::default_dir_perm) const noexcept
      -> os::Result<utils::file_descriptor>;

    [[nodiscard]] auto
    create_file(const std::filesystem::path &path,
                utils::bitflags<os::sys::open_flag> flags = os::sys::open_flag::none,
                std::filesystem::perms perm = os::default_new_file_perm) const noexcept
      -> os::Result<utils::file_descriptor>;

    // Unified creation for inode types that do not need an fd back
    // (symlink, hardlink, fifo, character/block device).
    [[nodiscard]] auto create(const std::filesystem::path &path,
                              const inode_type &type) const noexcept -> os::Result<void>;

    [[nodiscard]] auto remove_file(const std::filesystem::path &path) const noexcept
      -> os::Result<void>;

    [[nodiscard]] auto remove_dir(const std::filesystem::path &path) const noexcept
      -> os::Result<void>;

    [[nodiscard]] auto remove_all(const std::filesystem::path &path) const noexcept
      -> os::Result<void>;

    [[nodiscard]] auto rename(const std::filesystem::path &source,
                              const std::filesystem::path &destination,
                              os::sys::rename_flag flag = os::sys::rename_flag::none) const noexcept
      -> os::Result<void>;

    [[nodiscard]] auto readlink(const std::filesystem::path &path) const noexcept
      -> os::Result<std::filesystem::path>;

private:
    explicit Root(utils::file_descriptor fd) noexcept;
    [[nodiscard]] auto remove_inode(const std::filesystem::path &path, bool is_dir) const noexcept
      -> os::Result<void>;
    utils::file_descriptor fd_;
};

} // namespace linyaps_box::infra
