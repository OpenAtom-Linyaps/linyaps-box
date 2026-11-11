// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/resources.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <algorithm>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(resources,
                          resources_schema,
                          "resources",
                          field<&resources::unified>("unified").ignore_null(),
                          field<&resources::devices>("devices").ignore_null(),
                          field<&resources::pids_>("pids").ignore_null(),
                          field<&resources::memory_>("memory").ignore_null(),
                          field<&resources::cpu_>("cpu").ignore_null(),
                          field<&resources::hugepage_limits>("hugepageLimits").ignore_null(),
                          field<&resources::block_io_>("blockIO").ignore_null(),
                          field<&resources::network_>("network").ignore_null(),
                          field<&resources::rdma_>("rdma").ignore_null())

void validate(const resources &v)
{
    validate_if_present(v.memory_);
    validate_if_present(v.cpu_);
    validate_if_present(v.block_io_);
    validate_if_present(v.network_);

    if (v.rdma_) {
        std::for_each(v.rdma_->cbegin(), v.rdma_->cend(), [](const auto &entry) {
            validate(entry.second);
        });
    }

    if (v.hugepage_limits) {
        std::for_each(v.hugepage_limits->cbegin(),
                      v.hugepage_limits->cend(),
                      [](const auto &limit) {
                          validate(limit);
                      });
    }
}

} // namespace linyaps_box::config
