// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/hugepage.h"

#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, hugepage_limit &v)
{
    j.at("pageSize").get_to(v.page_size);
    j.at("limit").get_to(v.limit);
}

void validate(const hugepage_limit &v)
{
    if (UNLIKELY(v.page_size.empty())) {
        throw std::runtime_error("resources.hugepageLimits pageSize must not be empty");
    }

    const std::string_view s = v.page_size;
    const auto *it = s.cbegin();
    const auto *const end = s.cend();

    while (it != end && *it >= '0' && *it <= '9') {
        ++it;
    }

    if (UNLIKELY(it == s.cbegin())) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize must start with a number: {}", s));
    }

    if (it != end
        && (*it == 'K' || *it == 'k' || *it == 'M' || *it == 'm' || *it == 'G' || *it == 'g'
            || *it == 'T' || *it == 't')) {
        ++it;
    }

    if (UNLIKELY(it == end || (*it != 'B' && *it != 'b'))) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize must end with 'B': {}", s));
    }
    ++it;

    if (UNLIKELY(it != end)) {
        throw std::runtime_error(
          fmt::format("resources.hugepageLimits pageSize has invalid format: {}", s));
    }
}

} // namespace linyaps_box::config
