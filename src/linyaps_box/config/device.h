// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace linyaps_box::config {

struct device
{
    enum class type : std::uint8_t { character, unbuffered_character, block, fifo };

    type type_{ type::character };
    std::filesystem::path path;
    std::optional<uint32_t> major;
    std::optional<uint32_t> minor;
    std::optional<uint32_t> mode;
    std::optional<uint32_t> uid;
    std::optional<uint32_t> gid;
};

LINYAPS_REGISTER_ENUM_TABLE(device::type,
                            4,
                            { device::type::character, "c" },
                            { device::type::block, "b" },
                            { device::type::unbuffered_character, "u" },
                            { device::type::fifo, "p" })

void from_json(const nlohmann::json &j, device &v);

void validate(const device &v);

} // namespace linyaps_box::config
