// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <filesystem>

namespace linyaps_box::os {

[[nodiscard]] auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>;

auto set_child_subreaper(bool enabled) noexcept -> Result<void>;

auto set_keep_capabilities(bool enabled) noexcept -> Result<void>;

auto set_no_new_privileges(bool state) noexcept -> Result<void>;

auto clear_ambient_capability_set() noexcept -> Result<void>;

auto add_ambient_capability(long cap) noexcept -> Result<void>;

auto set_control_terminal(utils::file_descriptor_ref fd) noexcept -> Result<void>;

[[nodiscard]] auto umask(std::filesystem::perms perm) noexcept -> Result<std::filesystem::perms>;

[[nodiscard]] auto get_exit_code(int status) noexcept -> Result<int>;

} // namespace linyaps_box::os
