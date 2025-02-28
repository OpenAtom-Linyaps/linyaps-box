// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/printer.h"

namespace linyaps_box::impl {

class table_printer final : public virtual linyaps_box::printer
{
public:
    void print_status(const container_status_t &status) final;
    void print_statuses(const std::vector<container_status_t> &status) final;
};

static_assert(!std::is_abstract_v<table_printer>);

} // namespace linyaps_box::impl
