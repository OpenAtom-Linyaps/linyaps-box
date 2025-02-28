// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"
#include "linyaps_box/interface.h"

#include <vector>

namespace linyaps_box {
class status_directory : public virtual interface
{
public:
    virtual void write(const container_status_t &status) = 0;
    [[nodiscard]] virtual container_status_t read(const std::string &id) const = 0;
    virtual void remove(const std::string &id) = 0;
    [[nodiscard]] virtual std::vector<std::string> list() const = 0;
};
} // namespace linyaps_box
