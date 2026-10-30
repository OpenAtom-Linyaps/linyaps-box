// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace linyaps_box::io {

[[nodiscard]] auto read_all(utils::file_descriptor_ref fd, std::size_t hint = 0) noexcept
  -> os::Result<std::string>;

[[nodiscard]] auto read_all(const std::filesystem::path &path) noexcept -> os::Result<std::string>;

auto write_all(utils::file_descriptor_ref fd, utils::span<const std::byte> buf) noexcept
  -> os::Result<void>;

} // namespace linyaps_box::io
