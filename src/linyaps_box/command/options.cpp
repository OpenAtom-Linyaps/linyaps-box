// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/options.h"

#include "linyaps_box/config.h"
#include "linyaps_box/version.h"

#include <CLI/CLI.hpp>

#include <unistd.h>

linyaps_box::command::options linyaps_box::command::parse(int argc, char *argv[]) // NOLINT
{
    CLI::App app{ "A simple OCI runtime implementation focused on desktop applications.",
                  "ll-box" };
    app.set_version_flag("-v,--version", [] {
        std::stringstream ss;
        ss << "ll-box version " << LINYAPS_BOX_VERSION << "\n";
        ss << "spec " << linyaps_box::config::oci_version;
        return ss.str();
    });
    app.require_subcommand();

    linyaps_box::command::options options;

    auto default_root = std::filesystem::current_path().root_path() / "run" / "user"
            / std::to_string(geteuid()) / "linglong" / "box";

    if (auto *env = getenv("XDG_RUNTIME_DIR"); env != nullptr) {
        default_root = std::filesystem::path{ env } / "linglong" / "box";
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

    list_options list_opt{ options.global };
    auto *cmd_list = app.add_subcommand("list", "List know containers");
    cmd_list->add_option("-f,--format", list_opt.output_format, "Specify the output format")
            ->type_name("FORMAT")
            ->transform(CLI::CheckedTransformer(
                    std::unordered_map<std::string_view, list_options::output_format_t>{
                            { "json", list_options::output_format_t::json },
                            { "table", list_options::output_format_t::table },
                    }))
            ->default_val(list_options::output_format_t::table);

    run_options run_opt{ options.global };
    auto *cmd_run = app.add_subcommand("run", "Create and immediately start a container");
    cmd_run->add_option("CONTAINER", run_opt.ID, "The container ID")->required();
    cmd_run->add_option("-b,--bundle", run_opt.bundle, "Path to the OCI bundle")->default_val(".");
    cmd_run->add_option("-f,--config", run_opt.config, "Override the configuration file to use")
            ->default_val("config.json");

    exec_options exec_opt{ options.global };
    auto *cmd_exec = app.add_subcommand("exec", "Exec a command in a running container")
                             ->positionals_at_end();
    cmd_exec->add_option("-u,--user",
                         exec_opt.user,
                         "Specify the user, "
                         "for example `1000` for UID=1000 "
                         "or `1000:1000` for UID=1000 and GID=1000")
            ->type_name("UID[:GID]");
    cmd_exec->add_option("--cwd", exec_opt.cwd, "Current working directory.");
    // TODO: enable capabilities and no_new_privs support after rewrite exec,
    //      cmd_exec->add_option("-c,--cap", options.exec.caps, "Set capabilities")
    //              ->check(
    //                      [](const std::string &val) -> std::string {
    //                          if (val.rfind("CAP_", 0) != std::string::npos) {
    //                              return "";
    //                          }

    //                         return "invalid argument, capability must start with CAP_";
    //                     },
    //                     "cap_check");
    //     cmd_exec->add_flag("--no-new-privs",
    //                        options.exec.no_new_privs,
    //                        "Set the no new privileges value for the process")
    //             ->default_val(false);
    cmd_exec->add_option("CONTAINER", exec_opt.ID, "Container ID")->required();
    cmd_exec->add_option("COMMAND", exec_opt.command, "Command to execute")->required();

    kill_options kill_opt{ options.global };
    auto *cmd_kill =
            app.add_subcommand("kill", "Send the specified signal to the container init process");
    cmd_kill->add_option("CONTAINER", kill_opt.container, "The container ID")->required();
    cmd_kill->add_option("SIGNAL", kill_opt.signal, "Signal to send")->default_val("SIGTERM");

    argv = app.ensure_utf8(argv);
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        options.global.return_code = app.exit(e);
    }

    if (cmd_list->parsed()) {
        options.subcommand_opt = list_opt;
    } else if (cmd_run->parsed()) {
        options.subcommand_opt = run_opt;
    } else if (cmd_exec->parsed()) {
        options.subcommand_opt = exec_opt;
    } else if (cmd_kill->parsed()) {
        options.subcommand_opt = kill_opt;
    }

    return options;
}
