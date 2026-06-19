// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"

#include <filesystem>
#include <string_view>

namespace linyaps_box {

class status_directory
{
public:
    explicit status_directory(std::filesystem::path path);

    auto write(const container_status_t &status) const -> void;
    [[nodiscard]] auto read() const -> container_status_t;
    auto remove() const -> void;

    auto write_config(std::string_view config) const -> void;
    auto save_config(const std::filesystem::path &src) const -> void;
    [[nodiscard]] auto config() const -> std::filesystem::path;

private:
    std::filesystem::path path_;
};

} // namespace linyaps_box
