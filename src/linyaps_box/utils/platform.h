// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include <filesystem>
#include <string_view>

namespace linyaps_box::utils {
auto str_to_signal(std::string_view str) -> int;
auto str_to_rlimit(std::string_view str) -> int;
auto get_path_max(const std::filesystem::path &fs_dir) noexcept -> std::size_t;
} // namespace linyaps_box::utils
