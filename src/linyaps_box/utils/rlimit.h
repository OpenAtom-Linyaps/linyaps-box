// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/rlimit.h"

namespace linyaps_box::utils {

// TODO: move those converter to from utils module
[[nodiscard]] auto to_rlimit_resource(config::rlimit::type type) noexcept -> int;

} // namespace linyaps_box::utils
