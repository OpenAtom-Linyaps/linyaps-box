// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <filesystem>

namespace linyaps_box::config {

struct root
{
    std::filesystem::path path;

    // Missing or null readonly preserves this OCI default.
    bool readonly{ false };
};

void validate(const root &v);

auto ops_of(json::object_tag<root>) -> const json::frame_ops &;
} // namespace linyaps_box::config
