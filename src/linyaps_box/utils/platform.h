// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <string_view>

namespace linyaps_box::utils {
auto str_to_signal(std::string_view str) -> int;
auto get_page_size() noexcept -> std::size_t;
// see: https://pubs.opengroup.org/onlinepubs/9799919799/
auto is_invalid_env(std::string_view env) noexcept -> bool;
} // namespace linyaps_box::utils
