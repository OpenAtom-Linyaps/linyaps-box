// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/exec_cpu_affinity.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/format.h>

#include <algorithm>
#include <string_view>

namespace linyaps_box::config {

namespace {

[[nodiscard]] auto is_valid_cpu_affinity(std::string_view s) noexcept -> bool
{
    return std::all_of(s.cbegin(), s.cend(), [](char c) {
        return (c >= '0' && c <= '9') || c == ',' || c == ' ' || c == '-';
    });
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(exec_cpu_affinity,
                          exec_cpu_affinity_schema,
                          "exec_cpu_affinity",
                          field<&exec_cpu_affinity::initial>("initial").ignore_null(),
                          field<&exec_cpu_affinity::final>("final").ignore_null())

void validate(const exec_cpu_affinity &v)
{
    require(!v.initial || is_valid_cpu_affinity(*v.initial), [&] {
        return fmt::format("process.execCPUAffinity is invalid: {}", *v.initial);
    });

    require(!v.final || is_valid_cpu_affinity(*v.final), [&] {
        return fmt::format("process.execCPUAffinity is invalid: {}", *v.final);
    });
}

} // namespace linyaps_box::config
