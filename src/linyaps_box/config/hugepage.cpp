// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hugepage.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <stdexcept>

namespace linyaps_box::config {

namespace {

auto consume_page_size_digits(std::string_view s) -> std::string_view
{
    if (UNLIKELY(s.empty() || s.front() < '1' || s.front() > '9')) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize must start with a non-zero digit: {}", s));
    }

    const auto digits = s.find_first_not_of("0123456789");
    return digits == std::string_view::npos ? std::string_view{ } : s.substr(digits);
}

auto consume_page_size_unit(std::string_view s, std::string_view rest) -> std::string_view
{
    if (UNLIKELY(rest.empty()
                 || (rest.front() != 'K' && rest.front() != 'M' && rest.front() != 'G'))) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize must use an uppercase K/M/G unit: {}", s));
    }

    return rest.substr(1);
}

auto consume_page_size_byte(std::string_view s, std::string_view rest) -> std::string_view
{
    if (UNLIKELY(rest.empty() || rest.front() != 'B')) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize must end with 'B': {}", s));
    }

    return rest.substr(1);
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(hugepage_limit,
                          hugepage_limit_schema,
                          "hugepage_limit",
                          field<&hugepage_limit::page_size>("pageSize").required(),
                          field<&hugepage_limit::limit>("limit").required())

void validate(const hugepage_limit &v)
{
    const std::string_view s = v.page_size;
    auto rest = consume_page_size_digits(s);
    rest = consume_page_size_unit(s, rest);
    rest = consume_page_size_byte(s, rest);

    require(rest.empty(), "resources.hugepageLimits pageSize has invalid format: {}", s);
}

} // namespace linyaps_box::config
