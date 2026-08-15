// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/backend.h"

#include <memory>
#include <string_view>

namespace linyaps_box::log {

[[nodiscard]] auto make_sink(std::string_view log_dest, output_format fmt, bool cee_syslog)
  -> std::unique_ptr<sink>;

} // namespace linyaps_box::log
