// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/ns.h"

#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <stdexcept>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, ns &v)
{
    auto type_str = j.at("type").get<std::string_view>();
    auto opt = get_enum_table_from<ns::type>().from_name(type_str);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error(fmt::format("unknown namespace type: {}", type_str));
    }
    v.type_ = *opt;

    if (auto path_it = j.find("path"); path_it != j.end() && !path_it->is_null()) {
        auto path = path_it->get<std::string_view>();
        if (UNLIKELY(path.empty())) {
            throw std::runtime_error(
              fmt::format("namespace path must not be empty for type: {}", type_str));
        }

        auto &fs_path = v.path.emplace(path);
        if (UNLIKELY(!fs_path.is_absolute())) {
            throw std::runtime_error(
              fmt::format("namespace path must be absolute for type: {}, got: {}", type_str, path));
        }
    }
}

void validate(const std::vector<ns> &v)
{
    std::array<bool, 8> seen{ };
    for (const auto &entry : v) {
        const auto index = static_cast<std::size_t>(entry.type_);
        if (UNLIKELY(seen[index])) {
            throw std::runtime_error(fmt::format("duplicate namespace type: {}", entry.type_));
        }

        seen[index] = true;
    }
}

} // namespace linyaps_box::config
