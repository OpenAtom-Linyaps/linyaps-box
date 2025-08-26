// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"
#include "linyaps_box/interface.h"

#include <vector>

namespace linyaps_box {
class printer : public virtual interface
{
protected:
    printer() = default;

public:
    ~printer() override;

    printer(const printer &) = delete;
    auto operator=(const printer &) -> printer & = delete;
    printer(printer &&) = delete;
    auto operator=(printer &&) -> printer & = delete;

    virtual void print_status(const container_status_t &status) = 0;
    virtual void print_statuses(const std::vector<container_status_t> &status) = 0;
};
} // namespace linyaps_box
