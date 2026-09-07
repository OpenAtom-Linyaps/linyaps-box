// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/device.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, device &v)
{
    constexpr auto device_type_table = get_enum_table_from<device::type>();
    bool have_type{ false };
    bool have_path{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "type")) {
            auto type_str = val.get<std::string_view>();
            auto type_opt = device_type_table.from_name(type_str);
            if (UNLIKELY(!type_opt)) {
                throw std::runtime_error(
                  fmt::format("device.type must be one of c/b/u/p: {}", type_str));
            }

            v.type_ = *type_opt;
            have_type = true;
        } else if (key_matches(k, "path")) {
            val.get_to(v.path);
            have_path = true;
        } else if (key_matches(k, "fileMode")) {
            if (!val.is_null()) {
                val.get_to(v.mode.emplace());
            }
        } else if (key_matches(k, "major")) {
            if (!val.is_null()) {
                val.get_to(v.major.emplace());
            }
        } else if (key_matches(k, "minor")) {
            if (!val.is_null()) {
                val.get_to(v.minor.emplace());
            }
        } else if (key_matches(k, "uid")) {
            if (!val.is_null()) {
                val.get_to(v.uid.emplace());
            }
        } else if (key_matches(k, "gid")) {
            if (!val.is_null()) {
                val.get_to(v.gid.emplace());
            }
        }
    }

    if (UNLIKELY(!have_type)) {
        throw std::runtime_error("device.type is required");
    }
    if (UNLIKELY(!have_path)) {
        throw std::runtime_error("device.path is required");
    }
}

void validate(const device &v)
{
    if (UNLIKELY(!v.path.is_absolute())) {
        throw std::runtime_error(fmt::format("device.path must be an absolute path: {}", v.path));
    }

    if (UNLIKELY(v.type_ != device::type::fifo && (!v.major || !v.minor))) {
        throw std::runtime_error("device.major and device.minor are required unless type is p");
    }
}

} // namespace linyaps_box::config
