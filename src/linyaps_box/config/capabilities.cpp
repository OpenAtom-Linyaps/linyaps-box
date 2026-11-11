// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/capabilities.h"

#include "linyaps_box/json/parser.h"

#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(capabilities,
                          capabilities_schema,
                          "capabilities",
                          field<&capabilities::effective>("effective").ignore_null(),
                          field<&capabilities::ambient>("ambient").ignore_null(),
                          field<&capabilities::bounding>("bounding").ignore_null(),
                          field<&capabilities::inheritable>("inheritable").ignore_null(),
                          field<&capabilities::permitted>("permitted").ignore_null())

} // namespace linyaps_box::config
