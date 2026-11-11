// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"
#include "linyaps_box/utils/enum_traits.h"

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

enum class device_rule_access_flag : std::uint8_t {
    none = 0U,
    read = (1U << 0U),
    write = (1U << 1U),
    mknod = (1U << 2U),
};

LINYAPS_ENABLE_BITMASK_ENUM(device_rule_access_flag)

LINYAPS_REGISTER_ENUM_TABLE(device_rule_access_flag,
                            3,
                            { device_rule_access_flag::read, "r" },
                            { device_rule_access_flag::write, "w" },
                            { device_rule_access_flag::mknod, "m" })

struct device_rule
{
    enum class type : std::uint8_t { all, character, block };

    bool allow;
    std::optional<type> type_;
    std::optional<int64_t> major;
    std::optional<int64_t> minor;
    std::optional<utils::bitflags<device_rule_access_flag>> access;
};

LINYAPS_REGISTER_ENUM_TABLE(device_rule::type,
                            3,
                            { device_rule::type::all, "a" },
                            { device_rule::type::character, "c" },
                            { device_rule::type::block, "b" })

auto ops_of(json::object_tag<device_rule>) -> const json::frame_ops &;
} // namespace linyaps_box::config
