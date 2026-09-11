// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/file_describer.h"

#include <fcntl.h>
#include <unistd.h>

namespace linyaps_box::os {

namespace sys {
enum class pipe_flags : uint32_t {
    none = 0,
    cloexec = O_CLOEXEC,
    nonblock = O_NONBLOCK,
    direct = O_DIRECT,
};
LINYAPS_ENABLE_BITMASK_ENUM(pipe_flags);
LINYAPS_REGISTER_ENUM_TABLE(pipe_flags,
                            4,
                            { pipe_flags::none, "NONE" },
                            { pipe_flags::cloexec, "O_CLOEXEC" },
                            { pipe_flags::nonblock, "O_NONBLOCK" },
                            { pipe_flags::direct, "O_DIRECT" })
} // namespace sys

[[nodiscard]] auto pipe2(utils::bitflags<sys::pipe_flags> flags) noexcept
  -> Result<std::pair<utils::file_descriptor, utils::file_descriptor>>;

} // namespace linyaps_box::os
