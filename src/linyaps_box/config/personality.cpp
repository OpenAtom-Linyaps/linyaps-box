// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/personality.h"

#include "linyaps_box/json/parser.h"

#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(personality,
                          personality_schema,
                          "personality",
                          field<&personality::domain_>("domain").required(),
                          field<&personality::flags>("flags").ignore_null())

} // namespace linyaps_box::config
