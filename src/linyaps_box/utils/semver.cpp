// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/semver.h"

#include "linyaps_box/utils/utils.h"

#include <algorithm>
#include <charconv>
#include <ostream>
#include <system_error>

namespace linyaps_box::utils {
namespace {

bool is_valid_identifier_char(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-';
}

void validate_identifiers(std::string_view str, std::string_view field)
{
    if (UNLIKELY(field.empty())) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }

    const auto *begin = field.begin();
    while (begin != field.end()) {
        const auto *dot = std::find(begin, field.end(), '.');
        if (UNLIKELY(begin == dot)) {
            throw std::invalid_argument("invalid semver: " + std::string{ str });
        }

        for (const auto *it = begin; it != dot; ++it) {
            if (UNLIKELY(!is_valid_identifier_char(*it))) {
                throw std::invalid_argument("invalid semver: " + std::string{ str });
            }
        }

        if (dot - begin > 1 && *begin == '0') {
            auto all_digits = std::all_of(begin + 1, dot, [](char c) {
                return c >= '0' && c <= '9';
            });

            if (UNLIKELY(all_digits)) {
                throw std::invalid_argument("invalid semver: " + std::string{ str });
            }
        }

        if (dot == field.end()) {
            break;
        }

        begin = dot + 1;
    }
}

void parse_uint_segment(std::string_view str,
                        std::string_view::size_type begin,
                        std::string_view::size_type end,
                        unsigned int &out)
{
    if (UNLIKELY(begin >= str.size() || end > str.size() || begin >= end)) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }

    auto len = end - begin;
    if (UNLIKELY(len > 1 && str[begin] == '0')) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }

    int value{ 0 };
    auto [ptr, ec] = std::from_chars(str.data() + begin, str.data() + end, value);
    if (UNLIKELY(ec != std::errc{ } || value < 0 || ptr != str.data() + end)) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }

    out = static_cast<unsigned int>(value);
}

} // anonymous namespace

semver::semver(std::string_view str)
{
    auto dot1 = str.find('.');
    if (UNLIKELY(dot1 == std::string_view::npos)) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }
    parse_uint_segment(str, 0, dot1, major_);

    auto dot2 = str.find('.', dot1 + 1);
    if (UNLIKELY(dot2 == std::string_view::npos)) {
        throw std::invalid_argument("invalid semver: " + std::string{ str });
    }
    parse_uint_segment(str, dot1 + 1, dot2, minor_);

    auto patch_end = str.find_first_of("-+", dot2 + 1);
    if (patch_end == std::string_view::npos) {
        patch_end = str.size();
    }
    parse_uint_segment(str, dot2 + 1, patch_end, patch_);

    if (patch_end == str.size()) {
        return;
    }

    if (str[patch_end] == '+') {
        auto build = str.substr(patch_end + 1);
        validate_identifiers(str, build);
        build_ = build;
        return;
    }

    auto prerelease_begin = patch_end + 1;
    auto plus = str.find('+', prerelease_begin);
    auto prerelease =
      str.substr(prerelease_begin, plus == std::string_view::npos ? plus : plus - prerelease_begin);
    validate_identifiers(str, prerelease);
    prerelease_ = prerelease;

    if (plus != std::string_view::npos) {
        auto build = str.substr(plus + 1);
        validate_identifiers(str, build);
        build_ = build;
    }
}

semver::semver(unsigned int major,
               unsigned int minor,
               unsigned int patch,
               std::string prerelease,
               std::string build) noexcept
    : major_(major)
    , minor_(minor)
    , patch_(patch)
    , prerelease_(std::move(prerelease))
    , build_(std::move(build))
{
}

unsigned int semver::major() const noexcept
{
    return major_;
}

unsigned int semver::minor() const noexcept
{
    return minor_;
}

unsigned int semver::patch() const noexcept
{
    return patch_;
}

const std::string &semver::prerelease() const noexcept
{
    return prerelease_;
}

const std::string &semver::build() const noexcept
{
    return build_;
}

std::string semver::to_string() const
{
    auto result =
      std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);
    if (!prerelease_.empty()) {
        result += "-" + prerelease_;
    }

    if (!build_.empty()) {
        result += "+" + build_;
    }

    return result;
}

bool semver::is_compatible_with(const semver &other) const noexcept
{
    if (major_ != other.major_) {
        return false;
    }

    return !(*this < other);
}

int semver::compare_prerelease(const std::string &a, const std::string &b) noexcept
{
    if (a.empty() && b.empty()) {
        return 0;
    }

    if (a.empty()) {
        return 1;
    }

    if (b.empty()) {
        return -1;
    }

    auto a_it = a.begin();
    auto b_it = b.begin();
    auto a_end = a.end();
    auto b_end = b.end();

    while (a_it != a_end && b_it != b_end) {
        auto a_dot = std::find(a_it, a_end, '.');
        auto b_dot = std::find(b_it, b_end, '.');

        auto a_numeric = std::all_of(a_it, a_dot, [](char c) {
            return c >= '0' && c <= '9';
        });
        auto b_numeric = std::all_of(b_it, b_dot, [](char c) {
            return c >= '0' && c <= '9';
        });

        if (a_numeric && b_numeric) {
            unsigned long long a_val = 0;
            std::from_chars(&*a_it, &*a_dot, a_val);
            unsigned long long b_val = 0;
            std::from_chars(&*b_it, &*b_dot, b_val);

            if (a_val < b_val) {
                return -1;
            }

            if (a_val > b_val) {
                return 1;
            }
        } else if (a_numeric != b_numeric) {
            return a_numeric ? -1 : 1;
        } else {
            auto cmp = std::lexicographical_compare(a_it, a_dot, b_it, b_dot);
            if (cmp) {
                return -1;
            }

            cmp = std::lexicographical_compare(b_it, b_dot, a_it, a_dot);
            if (cmp) {
                return 1;
            }
        }

        a_it = (a_dot == a_end) ? a_end : a_dot + 1;
        b_it = (b_dot == b_end) ? b_end : b_dot + 1;
    }

    if (a_it == a_end && b_it == b_end) {
        return 0;
    }

    return (a_it == a_end) ? -1 : 1;
}

bool operator==(const semver &lhs, const semver &rhs) noexcept
{
    return lhs.major_ == rhs.major_ && lhs.minor_ == rhs.minor_ && lhs.patch_ == rhs.patch_
      && lhs.prerelease_ == rhs.prerelease_;
}

bool operator!=(const semver &lhs, const semver &rhs) noexcept
{
    return !(lhs == rhs);
}

bool operator<(const semver &lhs, const semver &rhs) noexcept
{
    if (lhs.major_ != rhs.major_) {
        return lhs.major_ < rhs.major_;
    }

    if (lhs.minor_ != rhs.minor_) {
        return lhs.minor_ < rhs.minor_;
    }

    if (lhs.patch_ != rhs.patch_) {
        return lhs.patch_ < rhs.patch_;
    }

    return semver::compare_prerelease(lhs.prerelease_, rhs.prerelease_) < 0;
}

bool operator<=(const semver &lhs, const semver &rhs) noexcept
{
    return !(rhs < lhs);
}

bool operator>(const semver &lhs, const semver &rhs) noexcept
{
    return rhs < lhs;
}

bool operator>=(const semver &lhs, const semver &rhs) noexcept
{
    return !(lhs < rhs);
}

std::ostream &operator<<(std::ostream &os, const semver &v)
{
    return os << v.to_string();
}

} // namespace linyaps_box::utils

namespace std {

size_t
hash<linyaps_box::utils::semver>::operator()(const linyaps_box::utils::semver &v) const noexcept
{
    auto h1 = hash<unsigned int>{ }(v.major());
    auto h2 = hash<unsigned int>{ }(v.minor());
    auto h3 = hash<unsigned int>{ }(v.patch());
    auto h4 = hash<string>{ }(v.prerelease());
    return h1 ^ (h2 << 1U) ^ (h3 << 2U) ^ (h4 << 3U);
}

} // namespace std
