// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/linux.h"
#include "linyaps_box/config/mount.h"
#include "linyaps_box/os/mount.h"
#include "linyaps_box/utils/enum_traits.h"

namespace linyaps_box::specconv {

[[nodiscard]] auto to_mount_flags(utils::bitflags<config::vfs_flag> flags) noexcept
  -> utils::bitflags<os::sys::mount_flag>;

[[nodiscard]] auto from_mount_flags(utils::bitflags<os::sys::mount_flag> flags) noexcept
  -> utils::bitflags<config::vfs_flag>;

[[nodiscard]] auto to_propagation(utils::bitflags<config::propagation_flag> flags) noexcept
  -> utils::bitflags<os::sys::propagation_flag>;

[[nodiscard]] auto to_rootfs_propagation(config::rootfs_propagation prop) noexcept
  -> utils::bitflags<os::sys::propagation_flag>;

} // namespace linyaps_box::specconv
