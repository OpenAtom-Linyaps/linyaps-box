// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/io_priority.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, io_priority &v)
{
    auto name = j.at("class").get<std::string_view>();
    auto opt = get_enum_table_from<io_priority::class_t>().from_name(name);
    if (UNLIKELY(!opt)) {
        throw std::runtime_error(fmt::format("unknown I/O priority class: {}", name));
    }

    v.class_ = *opt;
    v.priority = j.value("priority", 0);
}

void validate(const io_priority &v)
{
    if (UNLIKELY(v.priority < 0 || v.priority > 7)) {
        throw std::runtime_error(
          fmt::format("io priority must be in range [0, 7], got: {}", v.priority));
    }
}

} // namespace linyaps_box::config
