// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <string>

namespace linyaps_box::utils {

class semver
{
public:
    semver(unsigned int major,
           unsigned int minor,
           unsigned int patch,
           std::string prerelease = "",
           std::string build = "");

    explicit semver(const std::string &str);

    [[nodiscard]] auto major() const -> unsigned int;
    [[nodiscard]] auto minor() const -> unsigned int;
    [[nodiscard]] auto patch() const -> unsigned int;
    [[nodiscard]] auto prerelease() const -> const std::string &;
    [[nodiscard]] auto build() const -> const std::string &;

    [[nodiscard]] auto to_string() const -> std::string;
    [[nodiscard]] auto is_compatible_with(const semver &other) const -> bool;

private:
    unsigned int major_;
    unsigned int minor_;
    unsigned int patch_;

    std::string prerelease_;
    std::string build_;
};

} // namespace linyaps_box::utils
