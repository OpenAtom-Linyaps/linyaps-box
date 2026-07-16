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
    cgroup_manager_t manager{ cgroup_manager_t::disabled };
    std::filesystem::path root;
    std::vector<std::string> log;
    log::level log_level;
    log::output_format log_format;
    bool cee_syslog{ false };
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
    bool no_new_privs{ false };
    bool tty{ false };
    int preserve_fds{ 0 };
    std::vector<std::string> command;
    std::optional<user_spec> user;
#ifdef LINYAPS_BOX_ENABLE_CAP
    std::optional<std::vector<cap_value_t>> caps;
#endif
    std::string ID;
    std::optional<std::filesystem::path> cwd;
    std::vector<std::string> envs;
    std::optional<std::filesystem::path> console_socket;
    std::optional<std::filesystem::path> process_file;
};

struct run_options
{
    std::string ID;
    std::filesystem::path bundle;
    std::filesystem::path config;
    std::optional<std::filesystem::path> console_socket;
    int preserve_fds{ 0 };
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

    global_options global;
    subcommand_opt_t subcommand_opt;
};

auto parse(int argc, char *argv[]) noexcept -> std::optional<options>;

} // namespace linyaps_box::command
