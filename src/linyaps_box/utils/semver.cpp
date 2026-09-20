// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/semver.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <iterator>
#include <stdexcept>

namespace linyaps_box::utils {

invalid_semver::invalid_semver(const std::string &message)
    : std::invalid_argument(message)
{
}

namespace {

bool is_valid_identifier_char(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-';
}

void validate_identifiers(std::string_view input,
                          std::string_view field,
                          std::string_view label,
                          bool numeric_no_leading_zero)
{
    if (UNLIKELY(field.empty())) {
        throw invalid_semver(
          fmt::format("invalid semver '{}': {} must not be empty", input, label));
    }

    const auto *begin = field.cbegin();
    while (begin != field.cend()) {
        const auto *dot = std::find(begin, field.cend(), '.');
        if (UNLIKELY(begin == dot)) {
            throw invalid_semver(fmt::format("invalid semver '{}': {} contains an empty "
                                             "identifier at offset {}",
                                             input,
                                             label,
                                             std::distance(field.cbegin(), begin)));
        }

        for (const auto *it = begin; it != dot; ++it) {
            if (UNLIKELY(!is_valid_identifier_char(*it))) {
                throw invalid_semver(
                  fmt::format("invalid semver '{}': {} contains invalid character {:?} at "
                              "offset {} (allowed: ASCII alphanumerics and '-')",
                              input,
                              label,
                              *it,
                              std::distance(field.cbegin(), it)));
            }
        }

        if (numeric_no_leading_zero && std::distance(begin, dot) > 1 && *begin == '0') {
            auto all_digits = std::all_of(begin + 1, dot, [](char c) {
                return c >= '0' && c <= '9';
            });

            if (UNLIKELY(all_digits)) {
                throw invalid_semver(
                  fmt::format("invalid semver '{}': numeric {} identifier '{}' must not have "
                              "leading zeroes",
                              input,
                              label,
                              std::string{ begin, dot }));
            }
        }

        if (dot == field.cend()) {
            break;
        }

        begin = dot + 1;
        if (UNLIKELY(begin == field.cend())) {
            throw invalid_semver(
              fmt::format("invalid semver '{}': {} must not end with '.'", input, label));
        }
    }
}

void parse_uint_segment(std::string_view input,
                        std::string_view::size_type begin,
                        std::string_view::size_type end,
                        std::string_view label,
                        unsigned int &out)
{
    if (UNLIKELY(begin >= input.size() || end > input.size() || begin >= end)) {
        throw invalid_semver(
          fmt::format("invalid semver '{}': {} is missing (expected '<major>.<minor>.<patch>')",
                      input,
                      label));
    }

    const auto segment = input.substr(begin, end - begin);
    if (UNLIKELY(segment.size() > 1 && segment.front() == '0')) {
        throw invalid_semver(
          fmt::format("invalid semver '{}': {} must not have leading zeroes ('{}')",
                      input,
                      label,
                      segment));
    }

    unsigned int value{ 0 };
    auto [ptr, ec] = std::from_chars(segment.data(), segment.data() + segment.size(), value);
    if (UNLIKELY(ec == std::errc::result_out_of_range)) {
        throw invalid_semver(
          fmt::format("invalid semver '{}': {} is out of range ('{}')", input, label, segment));
    }

    if (UNLIKELY(ec != std::errc{ } || ptr != segment.data() + segment.size())) {
        throw invalid_semver(
          fmt::format("invalid semver '{}': {} must be a number, got '{}'", input, label, segment));
    }

    out = value;
}

int compare_prerelease(const std::string &a, const std::string &b) noexcept
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

    std::string_view a_rest{ a };
    std::string_view b_rest{ b };

    while (!a_rest.empty() && !b_rest.empty()) {
        const auto a_dot = a_rest.find('.');
        const auto b_dot = b_rest.find('.');
        const auto a_id = a_rest.substr(0, a_dot);
        const auto b_id = b_rest.substr(0, b_dot);

        const auto a_numeric = std::all_of(a_id.cbegin(), a_id.cend(), [](char c) {
            return c >= '0' && c <= '9';
        });

        const auto b_numeric = std::all_of(b_id.cbegin(), b_id.cend(), [](char c) {
            return c >= '0' && c <= '9';
        });

        if (a_numeric && b_numeric) {
            if (a_id.size() != b_id.size()) {
                return a_id.size() < b_id.size() ? -1 : 1;
            }

            if (a_id < b_id) {
                return -1;
            }

            if (b_id < a_id) {
                return 1;
            }
        } else if (a_numeric != b_numeric) {
            return a_numeric ? -1 : 1;
        } else {
            if (a_id < b_id) {
                return -1;
            }

            if (b_id < a_id) {
                return 1;
            }
        }

        a_rest.remove_prefix(a_id.size());
        if (!a_rest.empty()) {
            a_rest.remove_prefix(1);
        }

        b_rest.remove_prefix(b_id.size());
        if (!b_rest.empty()) {
            b_rest.remove_prefix(1);
        }
    }

    if (a_rest.empty() && b_rest.empty()) {
        return 0;
    }

    return a_rest.empty() ? -1 : 1;
}

} // anonymous namespace

semver::semver(std::string_view str)
{
    auto dot1 = str.find('.');
    if (UNLIKELY(dot1 == std::string_view::npos)) {
        throw invalid_semver(fmt::format("invalid semver '{}': missing '.' after the major "
                                         "version (expected '<major>.<minor>.<patch>')",
                                         str));
    }
    parse_uint_segment(str, 0, dot1, "major version", major_);

    auto dot2 = str.find('.', dot1 + 1);
    if (UNLIKELY(dot2 == std::string_view::npos)) {
        throw invalid_semver(fmt::format("invalid semver '{}': missing '.' after the minor "
                                         "version (expected '<major>.<minor>.<patch>')",
                                         str));
    }
    parse_uint_segment(str, dot1 + 1, dot2, "minor version", minor_);

    auto patch_end = str.find_first_of("-+", dot2 + 1);
    if (patch_end == std::string_view::npos) {
        patch_end = str.size();
    }
    parse_uint_segment(str, dot2 + 1, patch_end, "patch version", patch_);

    if (patch_end == str.size()) {
        return;
    }

    if (str[patch_end] == '+') {
        auto build = str.substr(patch_end + 1);
        validate_identifiers(str, build, "build metadata", false);
        build_ = build;
        return;
    }

    auto prerelease_begin = patch_end + 1;
    auto plus = str.find('+', prerelease_begin);
    auto prerelease =
      str.substr(prerelease_begin, plus == std::string_view::npos ? plus : plus - prerelease_begin);
    validate_identifiers(str, prerelease, "prerelease", true);
    prerelease_ = prerelease;

    if (plus != std::string_view::npos) {
        auto build = str.substr(plus + 1);
        validate_identifiers(str, build, "build metadata", false);
        build_ = build;
    }
}

semver::semver(unsigned int major,
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
    if (!prerelease_.empty()) {
        validate_identifiers(to_string(), prerelease_, "prerelease", true);
    }

    if (!build_.empty()) {
        validate_identifiers(to_string(), build_, "build metadata", false);
    }
}

unsigned int semver::major_version() const noexcept
{
    return major_;
}

unsigned int semver::minor_version() const noexcept
{
    return minor_;
}

unsigned int semver::patch_version() const noexcept
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

    return compare_prerelease(lhs.prerelease_, rhs.prerelease_) < 0;
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
    auto h1 = hash<unsigned int>{ }(v.major_version());
    auto h2 = hash<unsigned int>{ }(v.minor_version());
    auto h3 = hash<unsigned int>{ }(v.patch_version());
    auto h4 = hash<string>{ }(v.prerelease());
    return h1 ^ (h2 << 1U) ^ (h3 << 2U) ^ (h4 << 3U);
}

} // namespace std
