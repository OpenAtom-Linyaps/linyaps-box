// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace linyaps_box::command {

struct list_options
{
    enum class output_format_t : std::uint8_t {
        table,
        json,
    };

    output_format_t output_format{ output_format_t::table };
};

struct exec_options
{
    std::string user;
    std::string cwd;
    std::string ID;
    std::vector<std::string> command;
};

struct run_options
{
    std::string ID;
    std::string bundle;
    std::string config;
};

struct kill_options
{
    std::string container;
    int signal;
};

struct options
{
    enum class command_t : std::uint8_t {
        not_set,
        list,
        exec,
        run,
        kill,
    };

    command_t command{ command_t::not_set };
    std::filesystem::path root;
    std::optional<int> return_code;

    list_options list;
    exec_options exec;
    run_options run;
    kill_options kill;
};

// This function parses the command line arguments.
// It might print help or usage to stdout or stderr.
options parse(int argc, char *argv[]) noexcept; // NOLINT

} // namespace linyaps_box::command

std::stringstream &operator<<(std::stringstream &ss,
                              const linyaps_box::command::list_options::output_format_t &format);
