// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::os {

auto isatty(const utils::file_descriptor &fd) noexcept -> Result<bool>;

}
