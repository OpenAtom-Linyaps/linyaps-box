// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"

namespace linyaps_box::os {

auto sethostname(std::string_view name) noexcept -> os::Result<void>;

auto setdomainname(std::string_view name) noexcept -> os::Result<void>;

} // namespace linyaps_box::os
