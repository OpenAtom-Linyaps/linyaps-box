// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

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

void from_json(const nlohmann::json &j, user &v);

void validate(const user &v);

} // namespace linyaps_box::config
