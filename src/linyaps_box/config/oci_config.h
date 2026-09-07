// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/hooks.h"
#include "linyaps_box/config/linux.h"
#include "linyaps_box/config/mount.h"
#include "linyaps_box/config/process.h"
#include "linyaps_box/config/root.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace linyaps_box::config {

using namespace std::string_view_literals;

struct oci_config
{
    static constexpr auto version = "1.3.0"sv;

    static auto parse(std::string_view content) -> oci_config;
    static auto parse(const std::filesystem::path &path) -> oci_config;

    std::optional<process> process_;
    std::optional<std::string> hostname;
    std::optional<std::string> domainname;
    std::vector<mount> mounts;
    std::optional<linux> linux_;
    std::optional<hooks> hooks_;
    std::optional<root> root_;
    std::optional<std::unordered_map<std::string, std::string>> annotations;
};

void from_json(const nlohmann::json &j, oci_config &v);

void validate(const oci_config &v);

} // namespace linyaps_box::config
