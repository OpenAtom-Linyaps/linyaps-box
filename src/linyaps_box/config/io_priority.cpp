// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/io_priority.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(io_priority,
                          io_priority_schema,
                          "io_priority",
                          field<&io_priority::class_>("class").required(),
                          field<&io_priority::priority>("priority").required())

void validate(const io_priority &v)
{
    require(v.priority >= 0 && v.priority <= 7,
            "io priority must be in range [0, 7], got: {}",
            v.priority);
}

} // namespace linyaps_box::config
