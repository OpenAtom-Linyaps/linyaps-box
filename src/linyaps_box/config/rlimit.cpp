// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/rlimit.h"

#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(rlimit,
                          rlimit_schema,
                          "rlimit",
                          field<&rlimit::type_>("type").required(),
                          field<&rlimit::soft>("soft").required(),
                          field<&rlimit::hard>("hard").required())

} // namespace linyaps_box::config
