// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <cstdint>
#include <string>

namespace linyaps_box::config {

struct hugepage_limit
{
    std::string page_size;
    uint64_t limit;
};

void validate(const hugepage_limit &v);

auto ops_of(json::object_tag<hugepage_limit>) -> const json::frame_ops &;
} // namespace linyaps_box::config
