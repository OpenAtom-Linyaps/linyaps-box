// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/utils.h"

#include <sys/ioctl.h>

#include <termios.h>

namespace linyaps_box::os {
auto isatty(utils::file_descriptor_ref fd) noexcept -> bool;

auto tcgetattr(utils::file_descriptor_ref fd) noexcept -> Result<struct termios>;

namespace sys {

using optional_action_underlying_t = utils::shrink_macros_t<TCSANOW, TCSADRAIN, TCSAFLUSH>;

enum class optional_action : optional_action_underlying_t {
    now = TCSANOW,
    drain = TCSADRAIN,
    flush = TCSAFLUSH,
};
LINYAPS_REGISTER_ENUM_TABLE(optional_action,
                            3,
                            { optional_action::now, "TCSANOW" },
                            { optional_action::drain, "TCSADRAIN" },
                            { optional_action::flush, "TCSAFLUSH" })

} // namespace sys

auto tcsetattr(utils::file_descriptor_ref fd,
               sys::optional_action action,
               const struct termios &termios) noexcept -> Result<void>;

auto tcsetwinsize(utils::file_descriptor_ref fd, winsize size) noexcept -> Result<void>;

auto tcgetwinsize(utils::file_descriptor_ref fd) noexcept -> Result<winsize>;
} // namespace linyaps_box::os
