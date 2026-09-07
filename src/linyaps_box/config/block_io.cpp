// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/block_io.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, block_io::weight_device &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);

    if (auto it = j.find("weight"); it != j.end() && !it->is_null()) {
        it->get_to(v.weight.emplace());
    }

    if (auto it = j.find("leafWeight"); it != j.end() && !it->is_null()) {
        it->get_to(v.leaf_weight.emplace());
    }
}

void from_json(const nlohmann::json &j, block_io::throttle_device &v)
{
    j.at("major").get_to(v.major);
    j.at("minor").get_to(v.minor);
    j.at("rate").get_to(v.rate);
}

void from_json(const nlohmann::json &j, block_io &v)
{
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "weight")) {
            if (!val.is_null()) {
                val.get_to(v.weight.emplace());
            }
        } else if (key_matches(k, "leafWeight")) {
            if (!val.is_null()) {
                val.get_to(v.leaf_weight.emplace());
            }
        } else if (key_matches(k, "weightDevice")) {
            if (!val.is_null()) {
                val.get_to(v.weight_devices.emplace());
            }
        } else if (key_matches(k, "throttleReadBpsDevice")) {
            if (!val.is_null()) {
                val.get_to(v.throttle_read_bps_device.emplace());
            }
        } else if (key_matches(k, "throttleWriteBpsDevice")) {
            if (!val.is_null()) {
                val.get_to(v.throttle_write_bps_device.emplace());
            }
        } else if (key_matches(k, "throttleReadIOPSDevice")) {
            if (!val.is_null()) {
                val.get_to(v.throttle_read_iops_device.emplace());
            }
        } else if (key_matches(k, "throttleWriteIOPSDevice")) {
            if (!val.is_null()) {
                val.get_to(v.throttle_write_iops_device.emplace());
            }
        }
    }
}

void validate(const block_io::weight_device &v)
{
    if (UNLIKELY(!v.weight && !v.leaf_weight)) {
        throw std::runtime_error("block_io weightDevice requires weight or leafWeight");
    }
}

void validate(const block_io &v)
{
    if (!v.weight_devices) {
        return;
    }

    std::for_each(v.weight_devices->cbegin(), v.weight_devices->cend(), [](const auto &d) {
        validate(d);
    });
}

} // namespace linyaps_box::config
