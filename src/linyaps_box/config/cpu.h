// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/strict_json_fwd.h"

#include <cstdint>
#include <optional>
#include <string>

namespace linyaps_box::config {

struct cpu
{
    enum class idle : uint8_t { none, idle };
    std::optional<std::string> cpus;
    std::optional<std::string> mems;
    std::optional<uint64_t> shares;
    std::optional<int64_t> quota;
    std::optional<uint64_t> burst;
    std::optional<uint64_t> period;
    std::optional<int64_t> realtime_runtime;
    std::optional<uint64_t> realtime_period;
    std::optional<idle> idle_;
};

LINYAPS_REGISTER_ENUM_TABLE(cpu::idle, 2, { cpu::idle::none, "none" }, { cpu::idle::idle, "idle" })

void from_json(const utils::strict_json &j, cpu &v);

void validate(const cpu &v);

} // namespace linyaps_box::config
