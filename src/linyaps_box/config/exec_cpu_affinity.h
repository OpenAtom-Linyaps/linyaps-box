// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/strict_json_fwd.h"

#include <optional>
#include <string>

namespace linyaps_box::config {

struct exec_cpu_affinity
{
    std::optional<std::string> initial;
    std::optional<std::string> final;
};

void from_json(const utils::strict_json &j, exec_cpu_affinity &v);

void validate(const exec_cpu_affinity &v);

} // namespace linyaps_box::config
