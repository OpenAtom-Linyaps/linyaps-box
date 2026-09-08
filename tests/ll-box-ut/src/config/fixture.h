// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/oci_config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace linyaps_box::test {

[[nodiscard]] inline auto load_fixture(const char *name) -> std::string
{
    const std::ifstream in{ name };
    if (!in) {
        throw std::runtime_error{ std::string{ "cannot open fixture: " } + name };
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

inline constexpr std::string_view default_process =
  R"({"cwd": "/", "args": ["/bin/true"], "user": {"uid": 0, "gid": 0}})";

[[nodiscard]] inline auto config_with(std::string_view extra = "",
                                      std::string_view process = default_process) -> std::string
{
    std::string out = "{\n  \"ociVersion\": \"1.3.0\",\n";
    if (!process.empty()) {
        out += "  \"process\": ";
        out.append(process);
    }

    if (!extra.empty()) {
        if (!process.empty()) {
            out += ",\n";
        }

        out += "  ";
        out.append(extra);
    }

    out += "\n}";
    return out;
}

[[nodiscard]] inline auto parse_config(std::string_view extra = "",
                                       std::string_view process = default_process)
  -> linyaps_box::config::oci_config
{
    return linyaps_box::config::oci_config::parse(std::string_view{ config_with(extra, process) });
}

} // namespace linyaps_box::test
