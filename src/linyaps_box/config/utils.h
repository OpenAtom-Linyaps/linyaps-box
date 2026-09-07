// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/utils.h"

#include <filesystem>
#include <string_view>

namespace linyaps_box::config {

[[nodiscard]] constexpr auto key_matches(std::string_view key, const char *name) noexcept -> bool
{
    const auto len = std::char_traits<char>::length(name);
    return key.size() == len && key.compare(0, len, name) == 0;
}

[[nodiscard]] auto read_json_to(const std::filesystem::path &path,
                                utils::uninit_vector<std::byte> &buffer) -> std::size_t;

[[nodiscard]] auto parse_range_list(std::string_view s) -> std::vector<unsigned int>;

} // namespace linyaps_box::config
