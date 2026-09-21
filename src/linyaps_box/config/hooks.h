// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/hook.h"
#include "linyaps_box/utils/strict_json_fwd.h"

#include <optional>
#include <vector>

namespace linyaps_box::config {

struct hooks
{
    std::optional<std::vector<hook>> prestart;
    std::optional<std::vector<hook>> create_runtime;
    std::optional<std::vector<hook>> create_container;
    std::optional<std::vector<hook>> start_container;
    std::optional<std::vector<hook>> poststart;
    std::optional<std::vector<hook>> poststop;
};

void from_json(const utils::strict_json &j, hooks &v);

void validate(const hooks &v);

} // namespace linyaps_box::config
