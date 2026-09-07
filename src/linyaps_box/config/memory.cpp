// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/memory.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, memory &v)
{
    // Single-pass traversal for perf
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "limit")) {
            if (!val.is_null()) {
                val.get_to(v.limit.emplace());
            }
        } else if (key_matches(k, "reservation")) {
            if (!val.is_null()) {
                val.get_to(v.reservation.emplace());
            }
        } else if (key_matches(k, "swap")) {
            if (!val.is_null()) {
                val.get_to(v.swap.emplace());
            }
        } else if (key_matches(k, "kernel")) {
            if (!val.is_null()) {
                val.get_to(v.kernel.emplace());
            }
        } else if (key_matches(k, "kernelTCP")) {
            if (!val.is_null()) {
                val.get_to(v.kernel_tcp.emplace());
            }
        } else if (key_matches(k, "swappiness")) {
            if (!val.is_null()) {
                val.get_to(v.swappiness.emplace());
            }
        } else if (key_matches(k, "disableOOMKiller")) {
            if (!val.is_null()) {
                val.get_to(v.disable_OOM_killer.emplace());
            }
        } else if (key_matches(k, "useHierarchy")) {
            if (!val.is_null()) {
                val.get_to(v.use_hierarchy.emplace());
            }
        } else if (key_matches(k, "checkBeforeUpdate")) {
            if (!val.is_null()) {
                val.get_to(v.check_before_update.emplace());
            }
        }
    }
}

void validate(const memory &v)
{
    if (UNLIKELY(v.swappiness && *v.swappiness > 100)) {
        throw std::runtime_error(
          fmt::format("memory.swappiness must be in range [0, 100], got: {}", *v.swappiness));
    }
}

} // namespace linyaps_box::config
