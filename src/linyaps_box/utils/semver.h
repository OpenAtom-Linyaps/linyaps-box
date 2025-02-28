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

    [[nodiscard]] unsigned int major() const;
    [[nodiscard]] unsigned int minor() const;
    [[nodiscard]] unsigned int patch() const;
    [[nodiscard]] const std::string &prerelease() const;
    [[nodiscard]] const std::string &build() const;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool is_compatible_with(const semver &other) const;

private:
    unsigned int major_;
    unsigned int minor_;
    unsigned int patch_;

    std::string prerelease_;
    std::string build_;
};

} // namespace linyaps_box::utils
