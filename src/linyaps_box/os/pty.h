// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::os {
auto unlockpt(utils::file_descriptor_ref fd) noexcept -> Result<void>;
auto ptsname(utils::file_descriptor_ref fd, utils::span<char> buf) noexcept
  -> Result<std::string_view>;
} // namespace linyaps_box::os
