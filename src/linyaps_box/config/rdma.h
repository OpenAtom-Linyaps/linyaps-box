// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <cstdint>
#include <optional>

namespace linyaps_box::config {

struct rdma
{
    std::optional<uint32_t> hca_handles;
    std::optional<uint32_t> hca_objects;
};

void validate(const rdma &v);

auto ops_of(json::object_tag<rdma>) -> const json::frame_ops &;
} // namespace linyaps_box::config
