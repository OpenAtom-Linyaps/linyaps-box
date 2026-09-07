// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace linyaps_box::config {

struct ns
{
    enum class type : std::uint8_t {
        ipc,
        uts,
        mount,
        pid,
        net,
        user,
        cgroup,
        time,
    };

    type type_;
    std::optional<std::filesystem::path> path;
};

LINYAPS_REGISTER_ENUM_TABLE(ns::type,
                            8,
                            { ns::type::ipc, "ipc" },
                            { ns::type::uts, "uts" },
                            { ns::type::mount, "mount" },
                            { ns::type::pid, "pid" },
                            { ns::type::net, "network" },
                            { ns::type::user, "user" },
                            { ns::type::cgroup, "cgroup" },
                            { ns::type::time, "time" })

void from_json(const nlohmann::json &j, ns &v);

void validate(const std::vector<ns> &v);

} // namespace linyaps_box::config
