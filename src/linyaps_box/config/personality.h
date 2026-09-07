// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

struct personality
{
    enum class domain : uint8_t { linux, linux32 };

    domain domain_;
    std::optional<std::vector<std::string>> flags;
};

LINYAPS_REGISTER_ENUM_TABLE(personality::domain,
                            2,
                            { personality::domain::linux, "LINUX" },
                            { personality::domain::linux32, "LINUX32" })

void from_json(const nlohmann::json &j, personality &v);

} // namespace linyaps_box::config
