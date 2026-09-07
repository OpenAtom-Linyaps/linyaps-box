// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace linyaps_box::config {

struct block_io
{
    struct weight_device
    {
        int64_t major;
        int64_t minor;
        std::optional<uint16_t> weight;
        std::optional<uint16_t> leaf_weight;
    };

    struct throttle_device
    {
        int64_t major;
        int64_t minor;
        uint64_t rate;
    };

    std::optional<uint16_t> weight;
    std::optional<uint16_t> leaf_weight;
    std::optional<std::vector<weight_device>> weight_devices;
    std::optional<std::vector<throttle_device>> throttle_read_bps_device;
    std::optional<std::vector<throttle_device>> throttle_write_bps_device;
    std::optional<std::vector<throttle_device>> throttle_read_iops_device;
    std::optional<std::vector<throttle_device>> throttle_write_iops_device;
};

void from_json(const nlohmann::json &j, block_io::weight_device &v);

void validate(const block_io::weight_device &v);

void from_json(const nlohmann::json &j, block_io::throttle_device &v);

void from_json(const nlohmann::json &j, block_io &v);

void validate(const block_io &v);

} // namespace linyaps_box::config
