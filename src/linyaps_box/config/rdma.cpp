// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/rdma.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(rdma,
                          rdma_schema,
                          "rdma",
                          field<&rdma::hca_handles>("hcaHandles").ignore_null(),
                          field<&rdma::hca_objects>("hcaObjects").ignore_null())

void validate(const rdma &v)
{
    require(v.hca_handles || v.hca_objects,
            "rdma requires at least one of hcaHandles or hcaObjects");
}

} // namespace linyaps_box::config
