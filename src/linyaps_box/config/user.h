// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/json/dsl_fwd.h"

#include <filesystem>
#include <optional>
#include <vector>

#include <sys/types.h>

namespace linyaps_box::config {

struct user
{
    uid_t uid{ 0 };
    gid_t gid{ 0 };
    std::optional<std::filesystem::perms> umask;
    std::optional<std::vector<gid_t>> additional_gids;
};

void validate(const user &v);

auto ops_of(json::object_tag<user>) -> const json::frame_ops &;
} // namespace linyaps_box::config
