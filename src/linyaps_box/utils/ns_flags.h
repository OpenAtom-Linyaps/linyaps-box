// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/ns.h"

#include <optional>
#include <vector>

namespace linyaps_box::utils {

// Consumer-side mapping (three-invariant model: config enums are self-declared,
// kernel values appear only on this side): namespace type -> clone(2) flag.
[[nodiscard]] auto to_clone_flag(config::ns::type type) noexcept -> unsigned int;

// Builds the clone(2) flags that CREATE new namespaces.  A namespace entry
// that carries a `path` is JOINED later via setns(2) (runc/crun/youki
// semantics), never created, so it contributes no flag.
[[nodiscard]] auto generate_clone_flags(
  const std::optional<std::vector<config::ns>> &namespaces) noexcept -> unsigned int;

} // namespace linyaps_box::utils
