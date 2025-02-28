// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/semver.h"

#include <stdexcept>
#include <utility>

linyaps_box::utils::semver::semver(const std::string &str)
{
    auto end = str.find('.');
    decltype(end) begin = 0;

    if (end == std::string::npos) {
        throw std::invalid_argument("invalid semver: " + str);
    }

    auto major = std::stoi(str.substr(begin, end));

    begin = end + 1;
    end = str.find('.', begin);
    if (end == std::string::npos) {
        throw std::invalid_argument("invalid semver: " + str);
    }

    auto minor = std::stoi(str.substr(begin, end));

    begin = end + 1;
    end = str.find_first_of("-+", begin);

    auto patch = std::stoi(str.substr(begin, end));

    if (end == std::string::npos) {
        this->major_ = major;
        this->minor_ = minor;
        this->patch_ = patch;
        return;
    }

    if (str[end] == '+') {
        this->major_ = major;
        this->minor_ = minor;
        this->patch_ = patch;
        this->build_ = str.substr(end + 1);
        return;
    }

    begin = end + 1;
    end = str.find('+', begin);

    auto prerelease = str.substr(begin, end);

    if (end == std::string::npos) {
        this->major_ = major;
        this->minor_ = minor;
        this->patch_ = patch;
        this->prerelease_ = prerelease;
        return;
    }

    auto build = str.substr(end + 1);
    this->major_ = major;
    this->minor_ = minor;
    this->patch_ = patch;
    this->prerelease_ = prerelease;
    this->build_ = build;
}

linyaps_box::utils::semver::semver(unsigned int major,
                                   unsigned int minor,
                                   unsigned int patch,
                                   std::string prerelease,
                                   std::string build)
    : major_(major)
    , minor_(minor)
    , patch_(patch)
    , prerelease_(std::move(prerelease))
    , build_(std::move(build))
{
}

const std::string &linyaps_box::utils::semver::prerelease() const
{
    return this->prerelease_;
}

const std::string &linyaps_box::utils::semver::build() const
{
    return this->build_;
}

std::string linyaps_box::utils::semver::to_string() const
{
    return std::to_string(this->major_) + "." + std::to_string(this->minor_) + "."
            + std::to_string(this->patch_) + (this->prerelease_.empty() ? "" : "-")
            + this->prerelease_ + (this->build_.empty() ? "" : "+") + this->build_;
}

bool linyaps_box::utils::semver::is_compatible_with(const semver &other) const
{
    if (this->major_ != other.major_) {
        return false;
    }

    if (this->minor_ < other.minor_) {
        return false;
    }

    if (this->minor_ > other.minor_) {
        return true;
    }

    if (this->patch_ < other.patch_) {
        return false;
    }

    if (this->patch_ > other.patch_) {
        return true;
    }

    // FIXME: handle prerelease

    return true;
}

[[nodiscard]] unsigned int linyaps_box::utils::semver::major() const
{
    return this->major_;
}

[[nodiscard]] unsigned int linyaps_box::utils::semver::minor() const
{
    return this->minor_;
}

[[nodiscard]] unsigned int linyaps_box::utils::semver::patch() const
{
    return this->patch_;
}
