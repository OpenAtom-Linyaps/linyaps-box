// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <filesystem>

namespace linyaps_box::os {

// currently we don't use any pidfd flags
// support these flags in the future if we need
[[nodiscard]] auto pidfd_open(pid_t pid) noexcept -> Result<utils::file_descriptor>;

// TODO: introducing strong typed signal
[[nodiscard]] auto pidfd_send_signal(utils::file_descriptor_ref pidfd, int signal) noexcept
  -> Result<void>;

[[nodiscard]] auto kill_process(pid_t pid, int signal) noexcept -> Result<void>;

[[nodiscard]] auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>;

[[nodiscard]] auto set_child_subreaper(bool enabled) noexcept -> Result<void>;

[[nodiscard]] auto set_keep_capabilities(bool enabled) noexcept -> Result<void>;

[[nodiscard]] auto set_no_new_privileges(bool state) noexcept -> Result<void>;

[[nodiscard]] auto clear_ambient_capability_set() noexcept -> Result<void>;

[[nodiscard]] auto add_ambient_capability(long cap) noexcept -> Result<void>;

[[nodiscard]] auto set_control_terminal(utils::file_descriptor_ref fd) noexcept -> Result<void>;

[[nodiscard]] auto umask(std::filesystem::perms perm) noexcept -> Result<std::filesystem::perms>;

[[nodiscard]] auto get_exit_code(int status) noexcept -> Result<int>;

} // namespace linyaps_box::os
