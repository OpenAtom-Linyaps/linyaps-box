// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace linyaps_box {

class status_directory
{
public:
    explicit status_directory(std::filesystem::path path);

    void write(const container_status_t &status) const;
    [[nodiscard]] auto read() const -> container_status_t;
    void remove() const;

    void write_config(std::string_view config) const;
    [[nodiscard]] auto read_config() const -> std::string;

private:
    std::filesystem::path path_;
};

} // namespace linyaps_box
