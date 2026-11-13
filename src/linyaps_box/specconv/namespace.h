// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/ns.h"

#include <optional>
#include <string_view>
#include <vector>

namespace linyaps_box::specconv {

[[nodiscard]] auto to_clone_flag(config::ns::type type) noexcept -> unsigned int;

[[nodiscard]] auto generate_clone_flags(
  const std::optional<std::vector<config::ns>> &namespaces) noexcept -> unsigned int;

[[nodiscard]] auto to_proc_ns_name(config::ns::type type) noexcept -> std::string_view;

} // namespace linyaps_box::specconv
