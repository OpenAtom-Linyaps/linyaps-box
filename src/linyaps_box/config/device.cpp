// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/device.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/std.h>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(device,
                          device_schema,
                          "device",
                          field<&device::type_>("type").required(),
                          field<&device::path>("path").required(),
                          field<&device::mode>("fileMode").ignore_null(),
                          field<&device::major>("major").ignore_null(),
                          field<&device::minor>("minor").ignore_null(),
                          field<&device::uid>("uid").ignore_null(),
                          field<&device::gid>("gid").ignore_null())

void validate(const device &v)
{
    require(v.path.is_absolute(), "device.path must be an absolute path: {}", v.path);
    require(v.type_ == device::type::fifo || (v.major && v.minor),
            "device.major and device.minor are required unless type is p");
}

} // namespace linyaps_box::config
