// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/network_device.h"

#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(network_device,
                          network_device_schema,
                          "network_device",
                          field<&network_device::name>("name").ignore_null())

} // namespace linyaps_box::config
