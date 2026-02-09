// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/container_status.h"
#include "linyaps_box/interface.h"

#include <vector>

namespace linyaps_box {
// TODO: place container status into a directory
class status_directory : public virtual interface
{
protected:
    status_directory() = default;

public:
    ~status_directory() override;

    status_directory(const status_directory &) = delete;
    auto operator=(const status_directory &) -> status_directory & = delete;
    status_directory(status_directory &&) = delete;
    auto operator=(status_directory &&) -> status_directory & = delete;

    virtual void write(const container_status_t &status) const = 0;
    [[nodiscard]] virtual auto read(const std::string &id) const -> container_status_t = 0;
    virtual void remove(const std::string &id) const = 0;
    [[nodiscard]] virtual auto list() const -> std::vector<std::string> = 0;
};
} // namespace linyaps_box
