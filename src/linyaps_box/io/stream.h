// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"
#include "linyaps_box/utils/utils.h"

namespace linyaps_box::io {

auto read_to_end(utils::file_descriptor_ref fd, utils::uninit_vector<std::byte> &buf) noexcept
  -> os::Result<std::size_t>;

auto write_all(utils::file_descriptor_ref fd, utils::span<const std::byte> buf) noexcept
  -> os::Result<void>;

} // namespace linyaps_box::io
