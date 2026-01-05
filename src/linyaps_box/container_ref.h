// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"
#include "linyaps_box/container_status.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/unixsocket.h"

namespace linyaps_box {

struct exec_container_option
{
    int preserve_fds;
    config::process_t proc;
    std::optional<unixSocketClient> console_socket;
};

class container_ref
{
public:
    container_ref(const status_directory &status_dir, std::string id);
    virtual ~container_ref() noexcept;

    container_ref(const container_ref &) = delete;
    auto operator=(const container_ref &) -> container_ref & = delete;
    container_ref(container_ref &&) = delete;
    auto operator=(container_ref &&) -> container_ref & = delete;

    [[nodiscard]] auto status() const -> container_status_t;
    void kill(int signal) const;
    [[nodiscard]] auto exec(exec_container_option option) -> int;

protected:
    [[nodiscard]] auto status_dir() const -> const status_directory &;
    [[nodiscard]] auto get_id() const -> const std::string &;

private:
    std::string id_;
    const status_directory &status_dir_;
};

} // namespace linyaps_box
