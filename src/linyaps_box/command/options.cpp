// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/options.h"

#include "linyaps_box/config/oci_config.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/utils/environ.h"
#include "linyaps_box/version.h"

#include <CLI/CLI.hpp>

#include <array>
#include <charconv>
#include <csignal>

#include <unistd.h>

namespace {

struct SignalItem
{
    std::string_view name;
    int value;
};

auto str_to_signal(std::string_view str) noexcept -> int
{
    if (str.rfind("SIG", 0) != std::string_view::npos) {
        str.remove_prefix(3);
    }

    static constexpr std::array sig_list{
        SignalItem{ "ABRT", SIGABRT },     SignalItem{ "ALRM", SIGALRM },
        SignalItem{ "BUS", SIGBUS },       SignalItem{ "CHLD", SIGCHLD },
#ifdef SIGCLD
        SignalItem{ "CLD", SIGCLD }, // alias for CHLD
#endif
        SignalItem{ "CONT", SIGCONT },     SignalItem{ "FPE", SIGFPE },
        SignalItem{ "HUP", SIGHUP },       SignalItem{ "ILL", SIGILL },
        SignalItem{ "INT", SIGINT },       SignalItem{ "IO", SIGIO },
        SignalItem{ "IOT", SIGIOT },       SignalItem{ "KILL", SIGKILL },
        SignalItem{ "PIPE", SIGPIPE },     SignalItem{ "POLL", SIGPOLL },
        SignalItem{ "PROF", SIGPROF },     SignalItem{ "PWR", SIGPWR },
        SignalItem{ "QUIT", SIGQUIT },     SignalItem{ "SEGV", SIGSEGV },
        SignalItem{ "STOP", SIGSTOP },     SignalItem{ "SYS", SIGSYS },
        SignalItem{ "TERM", SIGTERM },     SignalItem{ "TRAP", SIGTRAP },
        SignalItem{ "TSTP", SIGTSTP },     SignalItem{ "TTIN", SIGTTIN },
        SignalItem{ "TTOU", SIGTTOU },     SignalItem{ "URG", SIGURG },
        SignalItem{ "USR1", SIGUSR1 },     SignalItem{ "USR2", SIGUSR2 },
        SignalItem{ "VTALRM", SIGVTALRM }, SignalItem{ "WINCH", SIGWINCH },
        SignalItem{ "XCPU", SIGXCPU },     SignalItem{ "XFSZ", SIGXFSZ }
    };

    constexpr auto sorted [[maybe_unused]] = []() noexcept {
        for (size_t i = 1; i < sig_list.size(); ++i) {
            if (sig_list[i - 1].name >= sig_list[i].name) {
                return false;
            }
        }
        return true;
    }();
    static_assert(sorted, "signal list must be sorted alphabetically");

    const auto *it = std::lower_bound(sig_list.cbegin(),
                                      sig_list.cend(),
                                      str,
                                      [](const SignalItem &item, std::string_view val) -> bool {
                                          return item.name < val;
                                      });
    if (it == sig_list.cend() || it->name != str) {
        return -1;
    }

    return it->value;
}

auto default_root_path() -> std::filesystem::path
{
    static const auto default_root = [] {
        if (auto *env = ::getenv("XDG_RUNTIME_DIR"); env != nullptr) {
            return std::filesystem::path{ env } / "linglong" / "box";
        }

        return std::filesystem::path("/run/user") / fmt::format("{}/linglong/box", ::geteuid());
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
      ->type_name("SOCKET");
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
      ->envname("LINYAPS_BOX_LOG_FORMAT")
      ->transform(CLI::CheckedTransformer(format_map, CLI::ignore_case))
      ->default_val(linyaps_box::log::output_format::text);

    std::string help = "Log destinations (stderr, [file:]PATH, syslog:ID";
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
    help += ", journald:ID";
#endif
    help += ")";

    app.add_option("--log", opts.log, std::move(help))
      ->type_name("SINK")
      ->envname("LINYAPS_BOX_LOG_DESTINATION")
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
      ->check([](const std::string &str) {
          cap_value_t val{ };
          if (cap_from_name(str.c_str(), &val) < 0) {
              throw CLI::ValidationError("--cap", "invalid capability: " + str);
          }

          return std::string{ };
      });
#endif
    cmd->add_flag("--no-new-privs",
                  opts.no_new_privs,
                  "Set the no new privileges value for the process");
    cmd
      ->add_option("-p,--process",
                   opts.process_file,
                   "Path to the process.json file to use. "
                   "When given, it is the complete process "
                   "spec: --env/--cwd/-u/-t/--cap/COMMAND are "
                   "ignored")
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    cmd->add_option("CONTAINER", opts.ID, "Container ID")->required();
    cmd->add_option("COMMAND", opts.command, "Command to execute");
    cmd->callback([&opts]() {
        if (opts.command.empty() && !opts.process_file) {
            throw CLI::ValidationError("At least one of COMMAND or --process must be provided");
        }

        if (!opts.command.empty() && opts.process_file) {
            throw CLI::ValidationError("COMMAND and --process are mutually exclusive");
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

          auto sig_num = str_to_signal(str);
          if (UNLIKELY(sig_num < 0)) {
              throw CLI::ValidationError("SIGNAL", "invalid signal: " + str);
          }

          std::array<char, std::numeric_limits<int>::max_digits10 + 1> buf; // NOLINT
          auto [end, err] = std::to_chars(buf.data(), buf.data() + buf.size(), sig_num);
          if (UNLIKELY(err != std::errc{ })) {
              throw std::logic_error("signal mapping error");
          }
          *end = '\0';

          return std::string{ buf.data() };
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
    data.app.set_version_flag("-v,--version",
                              fmt::format("ll-box version {}\nspec {}",
                                          LINYAPS_BOX_VERSION,
                                          linyaps_box::config::oci_config::version));
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
    linyaps_box::command::options opts{ std::monostate{ }, std::move(data.global) };
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
        return linyaps_box::command::options{ std::monostate{ }, data.global };
    }

    return convert_result(data);
}
