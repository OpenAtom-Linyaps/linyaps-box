// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <linyaps_box/cgroup_manager.h>

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

namespace linyaps_box::command {

struct global_options
{
    cgroup_manager_t manager{ cgroup_manager_t::disabled };
    std::filesystem::path root;
    int return_code{ 0 };
};

struct list_options
{
    enum class output_format_t : std::uint8_t { table, json };

    explicit list_options(global_options &global)
        : global_(global)
    {
    }

    output_format_t output_format{ output_format_t::table };
    std::reference_wrapper<global_options> global_;
};

struct exec_options
{
    explicit exec_options(global_options &global)
        : global_(global)
    {
    }

    bool no_new_privs{ false };
    bool tty{ false };
    int preserve_fds{ 0 };
    std::reference_wrapper<global_options> global_;
    std::vector<std::string> command;
    std::string user;
    std::optional<std::vector<std::string>> caps;
    std::string ID;
    std::optional<std::string> cwd;
    std::optional<std::vector<std::string>> envs;
    std::optional<std::string> console_socket;
};

struct run_options
{
    explicit run_options(global_options &global)
        : global_(global)
    {
    }

    std::reference_wrapper<global_options> global_;
    std::string ID;
    std::string bundle;
    std::string config;
    std::optional<std::string> console_socket;
    int preserve_fds{ 0 };
};

struct kill_options
{
    explicit kill_options(global_options &global)
        : global_(global)
    {
    }

    std::reference_wrapper<global_options> global_;
    std::string container;
    std::string signal;
};

struct options
{
    options()
        : global()
    {
    }

    using subcommand_opt_t =
            std::variant<std::monostate, list_options, exec_options, run_options, kill_options>;

    global_options global;
    subcommand_opt_t subcommand_opt;
};

// This function parses the command line arguments.
// It might print help or usage to stdout or stderr.
options parse(int argc, char *argv[]); // NOLINT

} // namespace linyaps_box::command
