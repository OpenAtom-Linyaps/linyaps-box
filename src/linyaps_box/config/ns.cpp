// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/ns.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep

#include <fmt/format.h>
#include <fmt/std.h>

#include <array>
#include <filesystem>
#include <string_view>

namespace linyaps_box::config {

namespace {

// The pure rule for a namespace path, shared by the JSON conversion and
// validate(const ns&).  Returns the failure reason, or an empty string when the
// path is valid.
[[nodiscard]] auto ns_path_error(const std::filesystem::path &path) -> std::string
{
    if (path.empty()) {
        return "namespace path must not be empty";
    }

    if (!path.is_absolute()) {
        return fmt::format("namespace path must be absolute, got: {}", path);
    }

    return { };
}

auto read_path(const json::detail::value_view &val) -> std::filesystem::path
{
    std::filesystem::path fs_path{ val.as_string_view() };
    if (auto reason = ns_path_error(fs_path); !reason.empty()) {
        // Keep decode_error: the route appends the exact field path.
        throw json::decode_error{ std::move(reason) };
    }
    return fs_path;
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(ns,
                          ns_schema,
                          "ns",
                          field<&ns::type_>("type").required(),
                          field<&ns::path, &read_path>("path").ignore_null())

void validate(const ns &v)
{
    if (v.path) {
        const auto reason = ns_path_error(*v.path);
        require(reason.empty(), "{}", reason);
    }
}

void validate(utils::span<const ns> v)
{
    std::array<bool, 8> seen{ };
    for (const auto &entry : v) {
        const auto index = static_cast<std::size_t>(entry.type_);
        require(!seen[index], "duplicate namespace type: {}", entry.type_);
        seen[index] = true;
        validate(entry);
    }
}

} // namespace linyaps_box::config
