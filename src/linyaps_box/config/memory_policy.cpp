// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/memory_policy.h"

#include "linyaps_box/config/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

namespace {

[[nodiscard]] constexpr auto has_node_tokens(std::string_view s) noexcept -> bool
{
    return s.find_first_not_of(" \t\n\v\f\r,") != std::string_view::npos;
}

} // namespace

void from_json(const nlohmann::json &j, memory_policy &v)
{
    auto mode_name = j.at("mode").get<std::string_view>();
    auto mode_opt = utils::enum_table_v<memory_policy::mode>.from_name(mode_name);
    if (UNLIKELY(!mode_opt)) {
        throw std::runtime_error(fmt::format("unknown memory policy mode: {}", mode_name));
    }
    v.mode_ = *mode_opt;

    if (auto nodes_it = j.find("nodes"); nodes_it != j.cend() && !nodes_it->is_null()) {
        nodes_it->get_to(v.nodes.emplace());
    }

    if (auto flags_it = j.find("flags"); flags_it != j.cend() && !flags_it->is_null()) {
        utils::bitflags<memory_policy_flag> flags;
        for (const auto &f : *flags_it) {
            const auto flag_str = f.get<std::string_view>();
            auto flag_opt = utils::enum_table_v<memory_policy_flag>.from_name(flag_str);
            if (UNLIKELY(!flag_opt)) {
                throw std::runtime_error(fmt::format("unknown memory policy flag: {}", flag_str));
            }

            flags |= *flag_opt;
        }

        v.flags = flags;
    }
}

void validate(const memory_policy &v)
{
    using mode = memory_policy::mode;

    if (UNLIKELY(v.flags.contains(memory_policy_flag::static_nodes)
                 && v.flags.contains(memory_policy_flag::relative_nodes))) {
        throw std::runtime_error("memoryPolicy flags MPOL_F_STATIC_NODES and MPOL_F_RELATIVE_NODES "
                                 "are mutually exclusive");
    }

    if (UNLIKELY(v.flags.contains(memory_policy_flag::numa_balancing) && v.mode_ != mode::bind
                 && v.mode_ != mode::preferred_many)) {
        throw std::runtime_error(
          "memoryPolicy flag MPOL_F_NUMA_BALANCING requires MPOL_BIND or MPOL_PREFERRED_MANY");
    }

    const auto has_nodes = v.nodes && has_node_tokens(*v.nodes);

    switch (v.mode_) {
    case mode::default_: {
        if (UNLIKELY(has_nodes)) {
            throw std::runtime_error("memoryPolicy mode MPOL_DEFAULT must not specify nodes");
        }
    } break;
    case mode::local: {
        if (UNLIKELY(has_nodes || v.flags.contains(memory_policy_flag::static_nodes)
                     || v.flags.contains(memory_policy_flag::relative_nodes))) {
            throw std::runtime_error(
              "memoryPolicy mode MPOL_LOCAL accepts neither nodes nor static/relative flags");
        }
    } break;
    case mode::preferred: {
        if (UNLIKELY(!has_nodes
                     && (v.flags.contains(memory_policy_flag::static_nodes)
                         || v.flags.contains(memory_policy_flag::relative_nodes)))) {
            throw std::runtime_error(
              "memoryPolicy mode MPOL_PREFERRED with empty nodes rejects static/relative flags");
        }
    } break;
    case mode::bind:
    case mode::interleave:
    case mode::preferred_many:
        [[fallthrough]];
    case mode::weighted_interleave: {
        if (UNLIKELY(!has_nodes)) {
            throw std::runtime_error(
              "memoryPolicy mode MPOL_BIND/MPOL_INTERLEAVE/MPOL_PREFERRED_MANY/"
              "MPOL_WEIGHTED_INTERLEAVE requires at least one node");
        }
    } break;
    }
}

} // namespace linyaps_box::config
