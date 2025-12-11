// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <unistd.h>

namespace linyaps_box::utils {

auto setsid() -> pid_t;

} // namespace linyaps_box::utils
