// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>

namespace linyaps_box::utils {

enum class cgroup_t : std::uint16_t { unified, legacy, hybrid };

cgroup_t get_cgroup_type();

} // namespace linyaps_box::utils
