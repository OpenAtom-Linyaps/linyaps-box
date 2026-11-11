// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/memory_policy.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/format.h>

#include <string_view>

namespace linyaps_box::config {

namespace {

[[nodiscard]] constexpr auto has_node_tokens(std::string_view s) noexcept -> bool
{
    return s.find_first_not_of(" \t\n\v\f\r,") != std::string_view::npos;
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(memory_policy,
                          memory_policy_schema,
                          "memory_policy",
                          field<&memory_policy::mode_>("mode").required(),
                          field<&memory_policy::nodes>("nodes").ignore_null(),
                          field<&memory_policy::flags>("flags").ignore_null())

void validate(const memory_policy &v)
{
    using mode = memory_policy::mode;
    const auto flags = v.flags.value_or(utils::bitflags<memory_policy_flag>{ });
    const bool has_nodes = v.nodes && has_node_tokens(*v.nodes);
    const bool has_static_or_relative = flags.contains(memory_policy_flag::static_nodes)
      || flags.contains(memory_policy_flag::relative_nodes);

    require(!(flags.contains(memory_policy_flag::static_nodes)
              && flags.contains(memory_policy_flag::relative_nodes)),
            "memoryPolicy flags MPOL_F_STATIC_NODES and MPOL_F_RELATIVE_NODES are mutually "
            "exclusive");
    require(!flags.contains(memory_policy_flag::numa_balancing) || v.mode_ == mode::bind
              || v.mode_ == mode::preferred_many,
            "memoryPolicy flag MPOL_F_NUMA_BALANCING requires MPOL_BIND or MPOL_PREFERRED_MANY");
    require(v.mode_ != mode::default_ || !has_nodes,
            "memoryPolicy mode MPOL_DEFAULT must not specify nodes");
    require(v.mode_ != mode::local || (!has_nodes && !has_static_or_relative),
            "memoryPolicy mode MPOL_LOCAL accepts neither nodes nor static/relative flags");
    require(v.mode_ != mode::preferred || has_nodes || !has_static_or_relative,
            "memoryPolicy mode MPOL_PREFERRED with empty nodes rejects static/relative flags");
    require((v.mode_ != mode::bind && v.mode_ != mode::interleave && v.mode_ != mode::preferred_many
             && v.mode_ != mode::weighted_interleave)
              || has_nodes,
            "memoryPolicy mode MPOL_BIND/MPOL_INTERLEAVE/MPOL_PREFERRED_MANY/"
            "MPOL_WEIGHTED_INTERLEAVE requires at least one node");
}

} // namespace linyaps_box::config
