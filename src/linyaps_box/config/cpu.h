// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace linyaps_box::config {

struct cpu
{
    enum class idle : uint8_t { none, idle };

    std::optional<uint64_t> shares;
    std::optional<int64_t> quota;
    std::optional<uint64_t> burst;
    std::optional<uint64_t> period;
    std::optional<int64_t> realtime_runtime;
    std::optional<uint64_t> realtime_period;
    std::optional<std::vector<unsigned int>> cpus;
    std::optional<std::vector<unsigned int>> mems;
    std::optional<idle> idle_;
};

void from_json(const nlohmann::json &j, cpu &v);

void validate(const cpu &v);

} // namespace linyaps_box::config
