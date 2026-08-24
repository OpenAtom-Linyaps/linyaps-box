// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <sys/ioctl.h>

#include <termios.h>

namespace linyaps_box::os {
auto isatty(utils::file_descriptor_ref fd) noexcept -> bool;

auto tcgetattr(utils::file_descriptor_ref fd) noexcept -> Result<struct termios>;

enum class optional_action : uint8_t {
    now = TCSANOW,
    drain = TCSADRAIN,
    flush = TCSAFLUSH,
};
auto tcsetattr(utils::file_descriptor_ref fd,
               optional_action action,
               const struct termios &termios) noexcept -> Result<void>;

auto tcsetwinsize(utils::file_descriptor_ref fd, winsize size) noexcept -> Result<void>;

auto tcgetwinsize(utils::file_descriptor_ref fd) noexcept -> Result<winsize>;
} // namespace linyaps_box::os
