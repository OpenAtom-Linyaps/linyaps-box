// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config.h"
#include "linyaps_box/container_status.h"
#include "linyaps_box/status_directory.h"
#include "linyaps_box/unix_socket.h"

#include <string>

#include <sys/types.h>

namespace linyaps_box {

struct exec_container_option
{
    int preserve_fds{ 0 };
    std::optional<oci_config::process_t> proc;

    // Per-field overrides applied on top of option.proc or config.json's process:
    std::optional<uid_t> uid;
    std::optional<gid_t> gid;
    std::optional<std::filesystem::path> cwd;
    std::optional<bool> tty;
    std::optional<bool> no_new_privs;
    std::vector<std::string> extra_envs;
    std::vector<std::string> command;
#ifdef LINYAPS_BOX_ENABLE_CAP
    std::optional<std::vector<cap_value_t>> caps;
#endif

    std::optional<unix_socket> console_socket;
};

class container_ref
{
public:
    container_ref(status_directory status_dir, std::string id);
    virtual ~container_ref() noexcept;

    container_ref(const container_ref &) = delete;
    auto operator=(const container_ref &) -> container_ref & = delete;
    container_ref(container_ref &&) = default;
    auto operator=(container_ref &&) -> container_ref & = default;

    [[nodiscard]] auto status() const -> container_status_t;
    void kill(int signal) const;
    [[nodiscard]] auto exec(exec_container_option option) const -> int;

protected:
    [[nodiscard]] auto status_dir() const -> const status_directory &;
    [[nodiscard]] auto get_id() const -> const std::string &;

private:
    std::string id_;
    status_directory status_dir_;
};

} // namespace linyaps_box
