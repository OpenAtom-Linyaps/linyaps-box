// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/console_size.h"

#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(console_size,
                          console_size_schema,
                          "console_size",
                          field<&console_size::height>("height").required(),
                          field<&console_size::width>("width").required())

} // namespace linyaps_box::config
