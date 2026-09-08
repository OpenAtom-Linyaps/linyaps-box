// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/memory_policy.h"

#include "linyaps_box/config/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, memory_policy &v)
{
    auto mode_name = j.at("mode").get<std::string_view>();
    auto mode_opt = get_enum_table_from<memory_policy::mode>().from_name(mode_name);
    if (UNLIKELY(!mode_opt)) {
        throw std::runtime_error(fmt::format("unknown memory policy mode: {}", mode_name));
    }
    v.mode_ = *mode_opt;

    if (auto nodes_it = j.find("nodes"); nodes_it != j.end() && !nodes_it->is_null()) {
        v.nodes = parse_range_list(nodes_it->get<std::string_view>());
    }

    if (auto flags_it = j.find("flags"); flags_it != j.end() && !flags_it->is_null()) {
        utils::bitflags<memory_policy_flag> flags;
        for (const auto &f : *flags_it) {
            const auto flag_str = f.get<std::string_view>();
            auto flag_opt = get_enum_table_from<memory_policy_flag>().from_name(flag_str);
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
    const auto has_nodes = v.nodes && !v.nodes->empty();

    if (v.mode_ == memory_policy::mode::default_ || v.mode_ == memory_policy::mode::local) {
        if (UNLIKELY(has_nodes)) {
            throw std::runtime_error(
              "memoryPolicy mode MPOL_DEFAULT/MPOL_LOCAL must not specify nodes");
        }
    } else if (v.mode_ == memory_policy::mode::bind || v.mode_ == memory_policy::mode::interleave
               || v.mode_ == memory_policy::mode::preferred_many
               || v.mode_ == memory_policy::mode::weighted_interleave) {
        if (UNLIKELY(!has_nodes)) {
            throw std::runtime_error(
              "memoryPolicy mode MPOL_BIND/MPOL_INTERLEAVE/MPOL_PREFERRED_MANY/"
              "MPOL_WEIGHTED_INTERLEAVE requires at least one node");
        }
    }
}

} // namespace linyaps_box::config
