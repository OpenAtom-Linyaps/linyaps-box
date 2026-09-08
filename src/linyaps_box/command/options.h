// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/utils.h"

#include <linyaps_box/cgroup_manager.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <sys/types.h>

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

namespace linyaps_box::command {

struct global_options
{
    std::filesystem::path root;
    std::vector<std::string> log;
    log::level log_level;
    log::output_format log_format;
    bool cee_syslog{ false };
    cgroup_manager_t manager{ cgroup_manager_t::disabled };
};

struct list_options
{
    enum class output_format_t : std::uint8_t { table, json };

    output_format_t output_format{ output_format_t::table };
};

struct user_spec
{
    uid_t uid{ };
    gid_t gid{ };
};

struct exec_options
{
    std::optional<std::filesystem::path> console_socket;
    std::optional<std::filesystem::path> process_file;
    std::optional<std::filesystem::path> cwd;
#ifdef LINYAPS_BOX_ENABLE_CAP
    std::optional<std::vector<std::string>> caps;
#endif
    std::string ID;
    std::vector<std::string> envs;
    std::vector<std::string> command;
    std::optional<user_spec> user;
    uint preserve_fds{ 0 };
    bool no_new_privs{ false };
    bool tty{ false };
};

struct run_options
{
    std::optional<std::filesystem::path> console_socket;
    std::filesystem::path bundle;
    std::filesystem::path config;
    std::string ID;
    uint preserve_fds{ 0 };
};

struct kill_options
{
    std::string container;
    int signal{ };
};

struct options
{
    using subcommand_opt_t =
      std::variant<std::monostate, list_options, exec_options, run_options, kill_options>;

    subcommand_opt_t subcommand_opt;
    global_options global;
};

auto parse(int argc, char **argv) noexcept -> std::optional<options>;

} // namespace linyaps_box::command
