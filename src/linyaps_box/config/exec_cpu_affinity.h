// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <optional>
#include <string>

namespace linyaps_box::config {

struct exec_cpu_affinity
{
    std::optional<std::string> initial;
    std::optional<std::string> final;
};

void validate(const exec_cpu_affinity &v);

auto ops_of(json::object_tag<exec_cpu_affinity>) -> const json::frame_ops &;
} // namespace linyaps_box::config
