// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>

namespace linyaps_box::utils {

enum class cgroup_t : std::uint8_t { unified, legacy, hybrid };

auto get_cgroup_type() -> cgroup_t;

} // namespace linyaps_box::utils
