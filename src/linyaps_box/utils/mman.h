// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <optional>

namespace linyaps_box::utils {
auto memfd_create(const char *name, unsigned int flags) -> file_descriptor;
auto mmap(void *addr,
          std::size_t length,
          int prot,
          int flags,
          std::optional<std::reference_wrapper<const file_descriptor>> fd,
          off_t offset) -> std::byte *;
auto munmap(std::byte *addr, std::size_t length) -> void;
} // namespace linyaps_box::utils
