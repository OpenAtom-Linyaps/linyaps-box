// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/status_directory.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace linyaps_box {

class status_directory_manager
{
public:
    explicit status_directory_manager(std::filesystem::path root);

    [[nodiscard]] auto list() const -> std::vector<std::string>;
    [[nodiscard]] auto get(std::string_view id) const -> status_directory;

private:
    std::filesystem::path root_;
};

} // namespace linyaps_box
