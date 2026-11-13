// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <optional>
#include <string>

namespace linyaps_box::specconv {

[[nodiscard]] auto to_capability([[maybe_unused]] const std::string &name) noexcept
  -> std::optional<int>;

} // namespace linyaps_box::specconv
