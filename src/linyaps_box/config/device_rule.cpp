// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/device_rule.h"

#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <string_view>

namespace linyaps_box::config {

namespace {

auto read_access(const json::detail::value_view &val) -> utils::bitflags<device_rule_access_flag>
{
    const auto access_str = val.as_string_view();
    utils::bitflags<device_rule_access_flag> access_flags;
    for (const auto c : access_str) {
        auto flag_opt =
          utils::enum_table_v<device_rule_access_flag>.from_name(std::string_view(&c, 1));
        if (UNLIKELY(!flag_opt)) {
            throw json::decode_error{
                fmt::format("device_rule.access must be a composition of r/w/m: {}", c)
            };
        }

        access_flags |= *flag_opt;
    }

    return access_flags;
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(device_rule,
                          device_rule_schema,
                          "device_rule",
                          field<&device_rule::allow>("allow").required(),
                          field<&device_rule::type_>("type").ignore_null(),
                          field<&device_rule::major>("major").ignore_null(),
                          field<&device_rule::minor>("minor").ignore_null(),
                          field<&device_rule::access, &read_access>("access").ignore_null())
} // namespace linyaps_box::config
