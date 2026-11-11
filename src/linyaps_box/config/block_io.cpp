// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/block_io.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <algorithm>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(block_io::weight_device,
                          block_io_weight_device_schema,
                          "block_io.weight_device",
                          field<&block_io::weight_device::major>("major").required(),
                          field<&block_io::weight_device::minor>("minor").required(),
                          field<&block_io::weight_device::weight>("weight").ignore_null(),
                          field<&block_io::weight_device::leaf_weight>("leafWeight").ignore_null())

LINYAPS_BOX_CONFIG_SCHEMA(block_io::throttle_device,
                          block_io_throttle_device_schema,
                          "block_io.throttle_device",
                          field<&block_io::throttle_device::major>("major").required(),
                          field<&block_io::throttle_device::minor>("minor").required(),
                          field<&block_io::throttle_device::rate>("rate").required())

LINYAPS_BOX_CONFIG_SCHEMA(
  block_io,
  block_io_schema,
  "block_io",
  field<&block_io::weight>("weight").ignore_null(),
  field<&block_io::leaf_weight>("leafWeight").ignore_null(),
  field<&block_io::weight_devices>("weightDevice").ignore_null(),
  field<&block_io::throttle_read_bps_device>("throttleReadBpsDevice").ignore_null(),
  field<&block_io::throttle_write_bps_device>("throttleWriteBpsDevice").ignore_null(),
  field<&block_io::throttle_read_iops_device>("throttleReadIOPSDevice").ignore_null(),
  field<&block_io::throttle_write_iops_device>("throttleWriteIOPSDevice").ignore_null())

void validate(const block_io::weight_device &v)
{
    require(v.weight || v.leaf_weight, "block_io weightDevice requires weight or leafWeight");
}

void validate(const block_io &v)
{
    if (!v.weight_devices) {
        return;
    }

    std::for_each(v.weight_devices->cbegin(), v.weight_devices->cend(), [](const auto &d) {
        validate(d);
    });
}

} // namespace linyaps_box::config
