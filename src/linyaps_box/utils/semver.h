// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>

namespace linyaps_box::utils {

class semver
{
public:
    semver(unsigned int major,
           unsigned int minor,
           unsigned int patch,
           std::string prerelease = "",
           std::string build = "") noexcept;

    explicit semver(std::string_view str);

    [[nodiscard]] unsigned int major() const noexcept;
    [[nodiscard]] unsigned int minor() const noexcept;
    [[nodiscard]] unsigned int patch() const noexcept;
    [[nodiscard]] const std::string &prerelease() const noexcept;
    [[nodiscard]] const std::string &build() const noexcept;

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] bool is_compatible_with(const semver &other) const noexcept;

    friend bool operator==(const semver &lhs, const semver &rhs) noexcept;
    friend bool operator!=(const semver &lhs, const semver &rhs) noexcept;
    friend bool operator<(const semver &lhs, const semver &rhs) noexcept;
    friend bool operator<=(const semver &lhs, const semver &rhs) noexcept;
    friend bool operator>(const semver &lhs, const semver &rhs) noexcept;
    friend bool operator>=(const semver &lhs, const semver &rhs) noexcept;

    friend std::ostream &operator<<(std::ostream &os, const semver &v);

private:
    static int compare_prerelease(const std::string &a, const std::string &b) noexcept;

    unsigned int major_;
    unsigned int minor_;
    unsigned int patch_;

    std::string prerelease_;
    std::string build_;
};

} // namespace linyaps_box::utils

namespace std {

template <>
struct hash<linyaps_box::utils::semver>
{
    [[nodiscard]] size_t operator()(const linyaps_box::utils::semver &v) const noexcept;
};

} // namespace std
