// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/exec_cpu_affinity.h"

#include "linyaps_box/utils/strict_json.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <algorithm>
#include <stdexcept>
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

void validate(const exec_cpu_affinity &v)
{
    if (v.initial) {
        const std::string_view val = v.initial.value();
        if (UNLIKELY(!is_valid_cpu_affinity(val))) {
            throw std::runtime_error(fmt::format("process.execCPUAffinity is invalid: {}", val));
        }
    }

    if (v.final) {
        const std::string_view val = v.final.value();
        if (UNLIKELY(!is_valid_cpu_affinity(val))) {
            throw std::runtime_error(fmt::format("process.execCPUAffinity is invalid: {}", val));
        }
    }
}

void from_json(const utils::strict_json &j, exec_cpu_affinity &v)
{
    utils::require_object(j);
    if (auto it = j.find("initial"); it != j.end() && !it->is_null()) {
        it->get_to(v.initial.emplace());
    }

    if (auto it = j.find("final"); it != j.end() && !it->is_null()) {
        it->get_to(v.final.emplace());
    }
}

} // namespace linyaps_box::config
