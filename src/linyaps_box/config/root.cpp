// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/root.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(root,
                          root_schema,
                          "root",
                          field<&root::path>("path").required(),
                          field<&root::readonly>("readonly").ignore_null())

void validate(const root &v)
{
    require(!v.path.empty(), "root.path must not be empty");
}

} // namespace linyaps_box::config
