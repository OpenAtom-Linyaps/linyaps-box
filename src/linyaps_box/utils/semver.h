// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>

namespace linyaps_box::utils {

class invalid_semver : public std::invalid_argument
{
public:
    explicit invalid_semver(const std::string &message);
    invalid_semver(const invalid_semver &) = default;
    invalid_semver(invalid_semver &&) noexcept = default;
    auto operator=(const invalid_semver &) -> invalid_semver & = default;
    auto operator=(invalid_semver &&) noexcept -> invalid_semver & = default;
    ~invalid_semver() noexcept override = default;
};

class semver
{
public:
    semver(unsigned int major,
           unsigned int minor,
           unsigned int patch,
           std::string prerelease = "",
           std::string build = "");

    explicit semver(std::string_view str);

    [[nodiscard]] unsigned int major_version() const noexcept;
    [[nodiscard]] unsigned int minor_version() const noexcept;
    [[nodiscard]] unsigned int patch_version() const noexcept;
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
