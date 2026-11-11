// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/pids.h"

#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(pids, pids_schema, "pids", field<&pids::limit>("limit").ignore_null())

} // namespace linyaps_box::config
