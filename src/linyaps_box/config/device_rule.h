// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct device_rule
{
    enum class type : std::uint8_t { all, character, block };

    enum class access_flag : std::uint8_t {
        none = 0U,
        read = (1U << 0),
        write = (1U << 1),
        mknod = (1U << 2),
    };

    bool allow;
    std::optional<type> type_;
    std::optional<int64_t> major;
    std::optional<int64_t> minor;
    std::optional<access_flag> access;
};

LINYAPS_ENABLE_BITMASK_ENUM(device_rule::access_flag);

LINYAPS_REGISTER_ENUM_TABLE(device_rule::type,
                            3,
                            { device_rule::type::all, "a" },
                            { device_rule::type::character, "c" },
                            { device_rule::type::block, "b" })

LINYAPS_REGISTER_ENUM_TABLE(device_rule::access_flag,
                            3,
                            { device_rule::access_flag::read, "r" },
                            { device_rule::access_flag::write, "w" },
                            { device_rule::access_flag::mknod, "m" })

void from_json(const nlohmann::json &j, device_rule &v);

} // namespace linyaps_box::config
