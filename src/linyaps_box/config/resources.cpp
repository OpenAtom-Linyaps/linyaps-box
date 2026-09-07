// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/resources.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, resources &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "unified")) {
            if (!val.is_null()) {
                val.get_to(v.unified.emplace());
            }
        } else if (key_matches(k, "devices")) {
            if (!val.is_null()) {
                val.get_to(v.devices.emplace());
            }
        } else if (key_matches(k, "pids")) {
            if (!val.is_null()) {
                val.get_to(v.pids_.emplace());
            }
        } else if (key_matches(k, "memory")) {
            if (!val.is_null()) {
                val.get_to(v.memory_.emplace());
            }
        } else if (key_matches(k, "cpu")) {
            if (!val.is_null()) {
                val.get_to(v.cpu_.emplace());
            }
        } else if (key_matches(k, "hugepageLimits")) {
            if (!val.is_null()) {
                val.get_to(v.hugepage_limits.emplace());
            }
        } else if (key_matches(k, "blockIO")) {
            if (!val.is_null()) {
                val.get_to(v.block_io_.emplace());
            }
        } else if (key_matches(k, "network")) {
            if (!val.is_null()) {
                val.get_to(v.network_.emplace());
            }
        } else if (key_matches(k, "rdma")) {
            if (!val.is_null()) {
                val.get_to(v.rdma_.emplace());
            }
        }
    }
}

void validate(const resources &v)
{
    if (v.memory_) {
        validate(*v.memory_);
    }

    if (v.cpu_) {
        validate(*v.cpu_);
    }

    if (v.block_io_) {
        validate(*v.block_io_);
    }

    if (v.network_) {
        validate(*v.network_);
    }

    if (!v.hugepage_limits) {
        return;
    }

    std::for_each(v.hugepage_limits->cbegin(), v.hugepage_limits->cend(), [](const auto &limit) {
        if (UNLIKELY(limit.page_size.empty())) {
            throw std::runtime_error("resources.hugepageLimits pageSize must not be empty");
        }
    });
}

} // namespace linyaps_box::config
