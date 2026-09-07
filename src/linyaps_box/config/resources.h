// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/block_io.h"
#include "linyaps_box/config/cpu.h"
#include "linyaps_box/config/device_rule.h"
#include "linyaps_box/config/hugepage.h"
#include "linyaps_box/config/memory.h"
#include "linyaps_box/config/network.h"
#include "linyaps_box/config/pids.h"
#include "linyaps_box/config/rdma.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace linyaps_box::config {

struct resources
{
    std::optional<std::vector<device_rule>> devices;
    std::optional<memory> memory_;
    std::optional<cpu> cpu_;
    std::optional<block_io> block_io_;
    std::optional<std::vector<hugepage_limit>> hugepage_limits;
    std::optional<network> network_;
    std::optional<pids> pids_;
    std::optional<std::unordered_map<std::string, rdma>> rdma_;
    std::optional<std::unordered_map<std::string, std::string>> unified;
};

void from_json(const nlohmann::json &j, resources &v);

void validate(const resources &v);

} // namespace linyaps_box::config
