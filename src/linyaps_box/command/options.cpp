// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/options.h"

#include "linyaps_box/config.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/version.h"

#include <CLI/CLI.hpp>

#include <array>
#include <charconv>
#include <csignal>

#include <unistd.h>

namespace {

auto socket_check(const std::string &str) noexcept -> std::string
{
    try {
        auto ret = linyaps_box::utils::lstat(str);
        if (!linyaps_box::utils::is_type(ret.st_mode, std::filesystem::file_type::socket)) {
            return "console-socket must be an existing socket file";
        }
    } catch (const std::system_error &e) {
        return e.what();
    }

    return "";
};

auto default_root_path() -> std::filesystem::path
{
    static const auto default_root = [] {
        if (auto *env = ::getenv("XDG_RUNTIME_DIR"); env != nullptr) {
            return std::filesystem::path{ env } / "linglong" / "box";
        }
        return std::filesystem::path("/run/user") / std::to_string(geteuid()) / "linglong" / "box";
    }();
    return default_root;
}

template <typename T>
auto add_console_socket(CLI::App *cmd, T &opt) -> CLI::Option *
{
    return cmd
      ->add_option("--console-socket",
                   opt,
                   "Path to an unix socket that will receive the master end of the console's "
                   "pseudoterminal")
      ->type_name("SOCKET")
      ->check(socket_check, "must be an existing socket file");
}

template <typename T>
auto add_preserve_fds(CLI::App *cmd, T &opt) -> CLI::Option *
{
    return cmd
      ->add_option("--preserve-fds", opt, "Pass N additional file descriptors to the container")
      ->type_name("N")
      ->check(CLI::NonNegativeNumber)
      ->default_val(0);
}

auto register_global(CLI::App &app, linyaps_box::command::global_options &opts) -> void
{
    app.add_option("--root", opts.root, "Root directory for storage of container state")
      ->default_val(default_root_path());
    static constexpr std::array cgroup_managers{
        std::pair{ "cgroupfs", linyaps_box::cgroup_manager_t::cgroupfs },
        std::pair{ "systemd", linyaps_box::cgroup_manager_t::systemd },
        std::pair{ "disabled", linyaps_box::cgroup_manager_t::disabled },
    };
    app.add_option("--cgroup-manager", opts.manager, "Cgroup manager to use")
      ->type_name("MANAGER")
      ->transform(CLI::CheckedTransformer(cgroup_managers))
      ->default_val(linyaps_box::cgroup_manager_t::disabled);

    static constexpr std::array level_map{
        std::pair{ "fatal", linyaps_box::log::level::fatal },
        std::pair{ "error", linyaps_box::log::level::error },
        std::pair{ "warn", linyaps_box::log::level::warn },
        std::pair{ "info", linyaps_box::log::level::info },
        std::pair{ "debug", linyaps_box::log::level::debug },
    };

    app.add_option("--log-level", opts.log_level, "Set log level (fatal/error/warn/info/debug)")
      ->type_name("LEVEL")
      ->transform(CLI::CheckedTransformer(level_map, CLI::ignore_case))
      ->envname("LINYAPS_BOX_LOG_LEVEL")
      ->default_val(LINYAPS_BOX_LOG_DEFAULT_LEVEL);

    static constexpr std::array format_map{
        std::pair{ "text", linyaps_box::log::output_format::text },
        std::pair{ "json", linyaps_box::log::output_format::json },
    };

    app.add_option("--log-format", opts.log_format, "Set log format: text (default) or json")
      ->type_name("FORMAT")
      ->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case))
      ->default_val(linyaps_box::log::output_format::text);

    std::string help = "Log destinations (stderr, [file:]PATH, syslog:ID";
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
    help += ", journald:ID";
#endif
    help += ")";

    app.add_option("--log", opts.log, std::move(help))
      ->type_name("SINK")
      ->check([](const std::string &s) -> std::string {
          if (s.empty()) {
              return "empty log destination";
          }

          if (s == "stderr") {
              return "";
          }

          const std::string_view sv{ s };
          auto idx = sv.find(':');
          if (idx == std::string_view::npos) {
              return "";
          }

          auto scheme = sv.substr(0, idx);
          auto content = sv.substr(idx + 1);
          if (content.empty()) {
              return fmt::format("empty {} destination", scheme);
          }

          bool ok = (scheme == "file" || scheme == "syslog");
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
          ok = ok || (scheme == "journald");
#endif
          if (ok) {
              return "";
          }

          return "unknown log destination: " + s;
      });

    app.add_flag("--cee-syslog",
                 opts.cee_syslog,
                 "Prefix syslog messages with @cee: when --log-format=json");
}

auto register_list(CLI::App &app, linyaps_box::command::list_options &opts) -> CLI::App *
{
    auto *cmd = app.add_subcommand("list", "List known containers");
    static constexpr std::array format_map{
        std::pair{ "json", linyaps_box::command::list_options::output_format_t::json },
        std::pair{ "table", linyaps_box::command::list_options::output_format_t::table },
    };
    cmd->add_option("-f,--format", opts.output_format, "Specify the output format")
      ->type_name("FORMAT")
      ->transform(CLI::CheckedTransformer(format_map))
      ->default_val(linyaps_box::command::list_options::output_format_t::table);
    return cmd;
}

auto register_run(CLI::App &app, linyaps_box::command::run_options &opts) -> CLI::App *
{
    auto *cmd = app.add_subcommand("run", "Create and immediately start a container");
    cmd->add_option("CONTAINER", opts.ID, "The container ID")->required();
    cmd->add_option("-b,--bundle", opts.bundle, "Path to the OCI bundle")
      ->default_val(".")
      ->check(CLI::ExistingDirectory);
    cmd->add_option("-f,--config", opts.config, "Override the configuration file to use")
      ->type_name("FILE")
      ->default_val("config.json");
    add_preserve_fds(cmd, opts.preserve_fds);
    add_console_socket(cmd, opts.console_socket);
    return cmd;
}

auto register_exec(CLI::App &app, linyaps_box::command::exec_options &opts) -> CLI::App *
{
    auto *cmd =
      app.add_subcommand("exec", "Exec a command in a running container")->positionals_at_end();
    cmd
      ->add_option_function<std::string>(
        "-u,--user",
        [&opts](const std::string &value) {
            linyaps_box::command::user_spec spec{ };
            auto colon = value.find(':');
            auto uid_len = colon == std::string::npos ? value.size() : colon;

            auto [uid_ptr, uid_ec] =
              std::from_chars(value.data(), value.data() + uid_len, spec.uid);
            if (uid_ec != std::errc{ } || uid_ptr != value.data() + uid_len) {
                throw CLI::ValidationError("--user",
                                           "invalid UID:" + std::make_error_code(uid_ec).message());
            }

            if (colon != std::string::npos) {
                auto [gid_ptr, gid_ec] =
                  std::from_chars(value.data() + colon + 1, value.data() + value.size(), spec.gid);
                if (gid_ec != std::errc{ } || gid_ptr != value.data() + value.size()) {
                    throw CLI::ValidationError("--user",
                                               "invalid GID: "
                                                 + std::make_error_code(gid_ec).message());
                }
            } else {
                spec.gid = spec.uid;
            }

            opts.user = spec;
        },
        "Specify the user, "
        "for example `1000` for UID=1000 "
        "or `1000:1000` for UID=1000 and GID=1000")
      ->type_name("UID[:GID]");
    cmd->add_option("--cwd", opts.cwd, "Current working directory.")->type_name("PATH");
    cmd
      ->add_option("-e,--env",
                   opts.envs,
                   "Environment variables to set, use -e KEY=VALUE -e KEY2=VALUE2 for multiple")
      ->type_name("ENV")
      ->check(
        [](const std::string &str) noexcept -> std::string {
            return linyaps_box::utils::is_invalid_env(str) ? "invalid env: " + str : "";
        },
        "check environment variables is valid or not");
    add_console_socket(cmd, opts.console_socket);
    cmd->add_flag("-t,--tty", opts.tty, "Allocate a pseudo-TTY");
    add_preserve_fds(cmd, opts.preserve_fds);

#ifdef LINYAPS_BOX_ENABLE_CAP
    cmd->add_option("-c,--cap", opts.caps, "Set capabilities")
      ->type_name("CAP")
      ->transform([](const std::string &str) -> std::string {
          cap_value_t val{ };
          if (cap_from_name(str.c_str(), &val) < 0) {
              throw CLI::ValidationError("--cap", "invalid capability: " + str);
          }
          return std::to_string(val);
      });
#endif
    cmd->add_flag("--no-new-privs",
                  opts.no_new_privs,
                  "Set the no new privileges value for the process");
    cmd->add_option("-p,--process", opts.process_file, "Path to the process.json file to use")
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    cmd->add_option("CONTAINER", opts.ID, "Container ID")->required();
    cmd->add_option("COMMAND", opts.command, "Command to execute");
    cmd->callback([&opts]() {
        if (opts.command.empty() && !opts.process_file) {
            throw CLI::ValidationError("At least one of COMMAND or --process must be provided");
        }
    });
    return cmd;
}

auto register_kill(CLI::App &app, linyaps_box::command::kill_options &opts) -> CLI::App *
{
    auto *cmd =
      app.add_subcommand("kill", "Send the specified signal to the container init process");
    cmd->add_option("CONTAINER", opts.container, "The container ID")->required();
    cmd->add_option("SIGNAL", opts.signal, "Signal to send")
      ->transform([](const std::string &str) -> std::string {
          int sig{ };
          auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), sig);
          if (ec == std::errc{ } && ptr == str.data() + str.size()) {
              if (sig < 0 || sig >= NSIG) {
                  throw CLI::ValidationError("SIGNAL", "signal number out of range: " + str);
              }

              return str;
          }

          try {
              return std::to_string(linyaps_box::utils::str_to_signal(str));
          } catch (const std::invalid_argument &) {
              throw CLI::ValidationError("SIGNAL", "invalid signal: " + str);
          }
      })
      ->default_val(SIGTERM);
    return cmd;
}

} // namespace

namespace {

struct cli_app_data
{
    cli_app_data()
        : app("A simple OCI runtime implementation focused on desktop applications.", "ll-box")
    {
    }

    CLI::App app;
    linyaps_box::command::global_options global;
    linyaps_box::command::list_options list_opts;
    linyaps_box::command::run_options run_opts;
    linyaps_box::command::exec_options exec_opts;
    linyaps_box::command::kill_options kill_opts;
    CLI::App *cmd_list{ nullptr };
    CLI::App *cmd_run{ nullptr };
    CLI::App *cmd_exec{ nullptr };
    CLI::App *cmd_kill{ nullptr };
};

void build_cli_app(cli_app_data &data)
{
    data.app.set_version_flag("-v,--version", [] {
        return std::string("ll-box version ") + LINYAPS_BOX_VERSION + "\nspec "
          + linyaps_box::oci_config::version + "\n";
    });
    data.app.require_subcommand(1);

    register_global(data.app, data.global);
    data.cmd_list = register_list(data.app, data.list_opts);
    data.cmd_run = register_run(data.app, data.run_opts);
    data.cmd_exec = register_exec(data.app, data.exec_opts);
    data.cmd_kill = register_kill(data.app, data.kill_opts);
}

void run_parse(CLI::App &app, int argc, char **argv)
{
    argv = app.ensure_utf8(argv);
    app.parse(argc, argv);
}

auto convert_result(cli_app_data &data) -> linyaps_box::command::options
{
    linyaps_box::command::options opts{ std::move(data.global), std::monostate{ } };
    if (data.cmd_list->parsed()) {
        opts.subcommand_opt = data.list_opts;
    } else if (data.cmd_run->parsed()) {
        opts.subcommand_opt = std::move(data.run_opts);
    } else if (data.cmd_exec->parsed()) {
        opts.subcommand_opt = std::move(data.exec_opts);
    } else if (data.cmd_kill->parsed()) {
        opts.subcommand_opt = std::move(data.kill_opts);
    }
    return opts;
}

} // namespace

auto linyaps_box::command::parse(int argc, char **argv) noexcept -> std::optional<options>
{
    cli_app_data data;
    build_cli_app(data);

    try {
        run_parse(data.app, argc, argv);
    } catch (const CLI::ParseError &e) {
        auto code = data.app.exit(e);
        if (code != 0) {
            return std::nullopt;
        }

        // Help/version — return success with monostate subcommand
        return linyaps_box::command::options{ data.global, std::monostate{ } };
    }

    return convert_result(data);
}
