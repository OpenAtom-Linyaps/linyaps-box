// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/status_directory.h"

#include <filesystem>
#include <vector>

namespace linyaps_box::impl {
class status_directory : public virtual linyaps_box::status_directory
{
public:
    void write(const container_status_t &status) const override;
    [[nodiscard]] container_status_t read(const std::string &id) const override;
    void remove(const std::string &id) const override;
    [[nodiscard]] std::vector<std::string> list() const override;

    explicit status_directory(const std::filesystem::path &path);

private:
    std::filesystem::path path;
};

static_assert(!std::is_abstract_v<status_directory>);

} // namespace linyaps_box::impl
