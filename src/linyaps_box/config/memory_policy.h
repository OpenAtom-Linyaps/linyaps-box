// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace linyaps_box::config {

enum class memory_policy_flag : std::uint8_t {
    none = 0U,
    numa_balancing = (1U << 0),
    relative_nodes = (1U << 1),
    static_nodes = (1U << 2),
};

LINYAPS_ENABLE_BITMASK_ENUM(memory_policy_flag);

LINYAPS_REGISTER_ENUM_TABLE(memory_policy_flag,
                            3,
                            { memory_policy_flag::numa_balancing, "MPOL_F_NUMA_BALANCING" },
                            { memory_policy_flag::relative_nodes, "MPOL_F_RELATIVE_NODES" },
                            { memory_policy_flag::static_nodes, "MPOL_F_STATIC_NODES" })

struct memory_policy
{
    enum class mode : uint8_t {
        default_,
        bind,
        interleave,
        weighted_interleave,
        preferred,
        preferred_many,
        local,
    };

    std::optional<std::vector<unsigned int>> nodes;
    utils::bitflags<memory_policy_flag> flags;
    mode mode_;
};

LINYAPS_REGISTER_ENUM_TABLE(memory_policy::mode,
                            7,
                            { memory_policy::mode::default_, "MPOL_DEFAULT" },
                            { memory_policy::mode::bind, "MPOL_BIND" },
                            { memory_policy::mode::interleave, "MPOL_INTERLEAVE" },
                            { memory_policy::mode::weighted_interleave,
                              "MPOL_WEIGHTED_INTERLEAVE" },
                            { memory_policy::mode::preferred, "MPOL_PREFERRED" },
                            { memory_policy::mode::preferred_many, "MPOL_PREFERRED_MANY" },
                            { memory_policy::mode::local, "MPOL_LOCAL" })

void from_json(const nlohmann::json &j, memory_policy &v);

void validate(const memory_policy &v);

} // namespace linyaps_box::config
