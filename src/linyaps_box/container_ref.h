// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"
#include "linyaps_box/container_status.h"
#include "linyaps_box/status_directory.h"

namespace linyaps_box {

class container_ref
{
public:
    container_ref(const status_directory &status_dir, std::string id);
    virtual ~container_ref() noexcept = default;

    [[nodiscard]] container_status_t status() const;
    void kill(int signal) const;
    [[noreturn]] void exec(const config::process_t &process);

protected:
    [[nodiscard]] const status_directory &status_dir() const;
    [[nodiscard]] const std::string &get_id() const;

private:
    std::string id;
    const status_directory *status_dir_;
};

} // namespace linyaps_box
