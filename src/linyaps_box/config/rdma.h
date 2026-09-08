// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct rdma
{
    std::optional<uint32_t> hca_handles;
    std::optional<uint32_t> hca_objects;
};

void from_json(const nlohmann::json &j, rdma &v);

void validate(const rdma &v);

} // namespace linyaps_box::config
