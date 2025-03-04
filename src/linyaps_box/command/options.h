// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <linyaps_box/cgroup_manager.h>

#include <filesystem>
#include <optional>
#include <vector>

namespace linyaps_box::command {

struct global_options
{
    enum class command_t : std::uint8_t { not_set, list, exec, run, kill };

    command_t command{ command_t::not_set };
    cgroup_manager_t manager{ cgroup_manager_t::cgroupfs };
    std::filesystem::path root;
    std::optional<int> return_code;
};

struct list_options
{
    // FIXME: if the underlying type of enum class is std::uint8_t,
    //  the mapping message of CLI11 transformer is incorrect
    //  use std::uint16_t for now
    enum class output_format_t : std::uint16_t { table, json };

    explicit list_options(const global_options &global)
        : global(global)
    {
    }

    const global_options &global;
    output_format_t output_format{ output_format_t::table };
};

struct exec_options
{
    explicit exec_options(const global_options &global)
        : global(global)
    {
    }

    const global_options &global;
    std::string user;
    std::string cwd;
    std::string ID;
    std::vector<std::string> command;
};

struct run_options
{
    explicit run_options(const global_options &global)
        : global(global)
    {
    }

    const global_options &global;
    std::string ID;
    std::string bundle;
    std::string config;
};

struct kill_options
{
    explicit kill_options(const global_options &global)
        : global(global)
    {
    }

    const global_options &global;
    std::string container;
    int signal{ -1 };
};

struct options
{
    options()
        : global()
        , list(global)
        , exec(global)
        , run(global)
        , kill(global)
    {
    }

    global_options global;
    list_options list;
    exec_options exec;
    run_options run;
    kill_options kill;
};

// This function parses the command line arguments.
// It might print help or usage to stdout or stderr.
options parse(int argc, char *argv[]) noexcept; // NOLINT

} // namespace linyaps_box::command
