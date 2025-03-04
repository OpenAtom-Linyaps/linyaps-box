// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/options.h"

#include "CLI/CLI.hpp"

#include <csignal>

#include <unistd.h>

linyaps_box::command::options linyaps_box::command::parse(int argc, char *argv[]) noexcept // NOLINT
{
    CLI::App app{ "A simple OCI runtime implementation focused on desktop applications.",
                  "ll-box" };
    argv = app.ensure_utf8(argv);

    linyaps_box::command::options options;

    auto default_root = std::filesystem::current_path().root_path() / "run" / "user"
            / std::to_string(geteuid()) / "linglong" / "box";

    if (getenv("XDG_RUNTIME_DIR") == nullptr) {
        default_root = std::filesystem::path{ getenv("XDG_RUNTIME_DIR") } / "linglong" / "box";
    }

    app.add_option("--root", options.global.root, "Root directory for storage of container state")
            ->default_val(default_root);
    app.add_option("--cgroup-manager", options.global.manager, "Cgroup manager to use")
            ->type_name("MANAGER")
            ->transform(
                    CLI::CheckedTransformer(std::unordered_map<std::string_view, cgroup_manager_t>{
                            { "cgroupfs", cgroup_manager_t::cgroupfs },
                            { "systemd", cgroup_manager_t::systemd },
                            { "disabled", cgroup_manager_t::disabled },
                    }))
            ->default_val(cgroup_manager_t::cgroupfs);

    app.require_subcommand();

    auto *cmd_list = app.add_subcommand("list", "List know containers");
    cmd_list->add_option("-f,--format", options.list.output_format, "Specify the output format")
            ->type_name("FORMAT")
            ->transform(CLI::CheckedTransformer(
                    std::unordered_map<std::string_view, list_options::output_format_t>{
                            { "json", list_options::output_format_t::json },
                            { "table", list_options::output_format_t::table },
                    }))
            ->default_val(list_options::output_format_t::table);

    auto *cmd_run = app.add_subcommand("run", "Create and immediately start a container");
    cmd_run->add_option("CONTAINER", options.run.ID, "The container ID")->required();
    cmd_run->add_option("-b,--bundle", options.run.bundle, "Path to the OCI bundle")
            ->default_val(".");
    cmd_run->add_option("-f,--config", options.run.config, "Override the configuration file to use")
            ->default_val("config.json");

    auto *cmd_exec = app.add_subcommand("exec", "Exec a command in a running container");
    cmd_exec->add_option("-u,--user",
                         options.exec.user,
                         "Specify the user, "
                         "for example `1000` for UID=1000 "
                         "or `1000:1000` for UID=1000 and GID=1000")
            ->type_name("UID[:GID]");
    cmd_exec->add_option("--cwd", options.exec.cwd, "Current working directory.");
    cmd_exec->add_option("CONTAINER", options.exec.ID, "Container ID")->required();
    cmd_exec->add_option("COMMAND", options.exec.command, "Command to execute")->required();

    auto *cmd_kill =
            app.add_subcommand("kill", "Send the specified signal to the container init process");
    cmd_kill->add_option("--signal", options.kill.signal)
            ->type_name("SIGNAL")
            ->default_val(SIGTERM)
            ->default_str("TERM")
            ->transform(CLI::CheckedTransformer(std::unordered_map<std::string_view, int>{
                    { "TERM", SIGTERM },
                    { "STOP", SIGSTOP },
            }));
    cmd_kill->add_option("CONTAINER", options.kill.container, "The container ID")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        options.global.return_code = app.exit(e);
    }

    if (cmd_list->parsed()) {
        options.global.command = global_options::command_t::list;
    } else if (cmd_run->parsed()) {
        options.global.command = global_options::command_t::run;
    } else if (cmd_exec->parsed()) {
        options.global.command = global_options::command_t::exec;
    } else if (cmd_kill->parsed()) {
        options.global.command = global_options::command_t::kill;
    }

    return options;
}
