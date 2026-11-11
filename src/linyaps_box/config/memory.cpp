// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/memory.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/log/macro.h"

#include <fmt/format.h>

#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

LINYAPS_BOX_CONFIG_SCHEMA(memory,
                          memory_schema,
                          "memory",
                          field<&memory::limit>("limit").ignore_null(),
                          field<&memory::reservation>("reservation").ignore_null(),
                          field<&memory::swap>("swap").ignore_null(),
                          field<&memory::kernel>("kernel").ignore_null(),
                          field<&memory::kernel_tcp>("kernelTCP").ignore_null(),
                          field<&memory::swappiness>("swappiness").ignore_null(),
                          field<&memory::disable_OOM_killer>("disableOOMKiller").ignore_null(),
                          field<&memory::use_hierarchy>("useHierarchy").ignore_null(),
                          field<&memory::check_before_update>("checkBeforeUpdate").ignore_null())

void validate(const memory &v)
{
    if (UNLIKELY(v.swappiness && *v.swappiness > 100)) {
        throw std::runtime_error(
          fmt::format("memory.swappiness must be in range [0, 100], got: {}", *v.swappiness));
    }
}

} // namespace linyaps_box::config
