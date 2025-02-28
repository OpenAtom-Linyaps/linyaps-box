// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"
#include "linyaps_box/printer.h"

namespace linyaps_box::impl {

class json_printer final : public virtual linyaps_box::printer
{
public:
    void print_status(const container_status_t &status) final;
    void print_statuses(const std::vector<container_status_t> &status) final;
};

static_assert(!std::is_abstract_v<json_printer>);

} // namespace linyaps_box::impl
