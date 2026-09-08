// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/container_monitor.h"
#include "linyaps_box/infra/process_handle.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h" // IWYU pragma: keep
#include "linyaps_box/os/process.h"
#include "linyaps_box/protocol/message_channel.h"
#include "linyaps_box/protocol/sync_socket_forwarder.h"
#include "linyaps_box/security/privilege.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/close_range.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/environ.h"
#include "linyaps_box/utils/rlimit.h"
#include "linyaps_box/utils/session.h"
#include "linyaps_box/utils/setns.h"
#include "linyaps_box/utils/utils.h"

#include <algorithm>

#include <sys/resource.h>

namespace linyaps_box {

namespace {

auto dedup_env(std::vector<std::string> &env) -> void
{
    std::vector<std::string> result;
    result.reserve(env.size());
    std::unordered_map<std::string, std::size_t> index;
    index.reserve(env.size());
    for (auto &entry : env) {
        const auto eq = entry.find('=');
        auto [it, inserted] = index.try_emplace(entry.substr(0, eq), result.size());
        if (inserted) {
            result.push_back(std::move(entry));
        } else {
            result[it->second] = std::move(entry);
        }
    }

    env = std::move(result);
}

auto resolve_final_process(linyaps_box::exec_container_option &option,
                           const linyaps_box::config::oci_config &config)
  -> linyaps_box::config::process &
{
    if (option.proc) {
        return *option.proc;
    }

    option.proc = config.process_ ? *config.process_ : linyaps_box::config::process{ };

    auto &proc = *option.proc;

    if (option.cwd) {
        proc.cwd = std::move(*option.cwd);
    }

    if (proc.cwd.empty()) {
        proc.cwd = "/";
    }

    if (option.tty) {
        proc.terminal = option.tty;
    }

    if (option.no_new_privs) {
        proc.no_new_privileges = option.no_new_privs;
    }

    if (!option.extra_envs.empty()) {
        if (!proc.env) {
            proc.env.emplace();
        }

        proc.env->insert(proc.env->end(),
                         std::make_move_iterator(option.extra_envs.begin()),
                         std::make_move_iterator(option.extra_envs.end()));
    }

    if (proc.env) {
        dedup_env(*proc.env);
    }

    if (!option.command.empty()) {
        proc.args = std::move(option.command);
    }

    if (option.uid) {
        proc.user_.uid = *option.uid;
    }

    if (option.gid) {
        proc.user_.gid = *option.gid;
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (option.caps) {
        if (!proc.capabilities_) {
            proc.capabilities_.emplace();
        }

        auto &caps = *proc.capabilities_;
        for (auto *set : { &caps.bounding, &caps.effective, &caps.permitted }) {
            if (!*set) {
                set->emplace();
            }

            (*set)->insert((*set)->end(), option.caps->cbegin(), option.caps->cend());
        }

        if (caps.inheritable) {
            if (!caps.ambient) {
                caps.ambient.emplace();
            }

            // kernel constraint: ambient ⊆ inheritable, so only raise the
            // requested caps that are already inheritable into the ambient set
            const auto &inheritable = *caps.inheritable;
            std::copy_if(option.caps->cbegin(),
                         option.caps->cend(),
                         std::back_inserter(*caps.ambient),
                         [&](const std::string &cap) {
                             return std::find(inheritable.cbegin(), inheritable.cend(), cap)
                               != inheritable.cend();
                         });
        }
    }
#endif

    return proc;
}

void child_setup_terminal(const linyaps_box::config::process &proc,
                          protocol::child_message_channel &sync)
{
    if (!proc.terminal) {
        return;
    }

    auto [slave, path, master] = linyaps_box::create_pty_pair();

    slave.setup_stdio();
    if (proc.console_size_) {
        slave.set_size({ proc.console_size_->height, proc.console_size_->width, 0, 0 });
    }

    auto console_fd = std::move(master).take();
    auto ref = console_fd.ref();
    sync.send_console_fd(ref);
}

void child_apply_environment(const linyaps_box::config::process &proc,
                             const linyaps_box::config::oci_config &config)
{
    ::clearenv();

    if (config.process_) {
        for (const auto &env : *config.process_->env) {
            if (linyaps_box::utils::is_invalid_env(env)) {
                continue;
            }
            auto eq = env.find('=');
            ::setenv(env.substr(0, eq).c_str(), env.substr(eq + 1).c_str(), 1);
        }
    }

    if (proc.env) {
        for (const auto &env : *proc.env) {
            if (linyaps_box::utils::is_invalid_env(env)) {
                continue;
            }
            auto eq = env.find('=');
            ::setenv(env.substr(0, eq).c_str(), env.substr(eq + 1).c_str(), 1);
        }
    }
}

void child_apply_rlimits(const linyaps_box::config::process &proc)
{
    if (!proc.rlimits) {
        return;
    }

    for (const auto &rl : *proc.rlimits) {
        auto resource = utils::to_rlimit_resource(rl.type_);
        const struct ::rlimit limit{ rl.soft, rl.hard };
        if (::setrlimit(resource, &limit) != 0) {
            _exit(EXIT_FAILURE);
        }
    }
}

[[noreturn]] auto exec_child_process(pid_t target_pid,
                                     const linyaps_box::config::oci_config &config,
                                     const linyaps_box::config::process &proc,
                                     uint preserve_fds,
                                     protocol::child_message_channel child_chan) -> void
{
    try {
        auto &logger = linyaps_box::log::global_logger::instance();
        logger.set_forwarder(
          std::make_unique<linyaps_box::protocol::sync_socket_forwarder>(child_chan));

        bool pid_ns{ false };
        if (config.linux_ && config.linux_->namespaces) {
            linyaps_box::utils::join_container_namespaces(target_pid, *config.linux_);
            pid_ns = std::any_of(config.linux_->namespaces->cbegin(),
                                 config.linux_->namespaces->cend(),
                                 [](const auto &ns) {
                                     return ns.type_ == linyaps_box::config::ns::type::pid;
                                 });
        }

        if (pid_ns) {
            auto grandchild = ::fork();
            if (UNLIKELY(grandchild < 0)) {
                throw std::system_error(errno, std::system_category(), "fork grandchild failed");
            }

            if (grandchild > 0) {
                child_chan.send_pid_report(static_cast<pid_t>(grandchild));
                _exit(EXIT_SUCCESS);
            }
        } else {
            child_chan.send_pid_report(::getpid());
        }

        child_chan.expect_proceed();

        linyaps_box::utils::setsid();

        if (proc.terminal.value_or(false)) {
            child_setup_terminal(proc, child_chan);
        }

        child_apply_environment(proc, config);

        child_apply_rlimits(proc);

        linyaps_box::utils::close_range(3U + static_cast<unsigned>(preserve_fds),
                                        std::numeric_limits<unsigned>::max(),
                                        CLOSE_RANGE_CLOEXEC);

        if (proc.user_.umask) {
            auto val = proc.user_.umask.value();
            os::throw_if_error(os::umask(val), fmt::format("failed to set umask {}", val));
        }

        security::privilege_context ctx{ proc.user_ };

        auto effective_caps = [&]() -> std::optional<config::capabilities> {
            const auto &base_caps = config.process_ ? config.process_->capabilities_ : std::nullopt;

            if (!proc.capabilities_) {
                return base_caps;
            }

            const auto &caps = *proc.capabilities_;
            auto all_empty = (!caps.effective || caps.effective->empty())
              && (!caps.bounding || caps.bounding->empty())
              && (!caps.inheritable || caps.inheritable->empty())
              && (!caps.permitted || caps.permitted->empty())
              && (!caps.ambient || caps.ambient->empty());

            if (all_empty) {
                return base_caps;
            }

            return proc.capabilities_;
        }();

        ctx.set_capabilities(std::move(effective_caps))
          .set_no_new_privs(proc.no_new_privileges.value_or(false));

        // change before we drop caps
        for (auto fd : { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO }) {
            if (::fchown(fd, proc.user_.uid, proc.user_.gid) != 0) {
                if (errno != EINVAL && errno != ENOSYS) {
                    throw std::system_error(errno, std::system_category(), "fchown");
                }
            }
        }

        ctx.apply();

        std::vector<const char *> c_args;
        c_args.reserve(proc.args.size() + 1);
        for (const auto &arg : proc.args) {
            c_args.push_back(arg.c_str());
        }
        c_args.push_back(nullptr);

        std::vector<const char *> c_env;
        if (proc.env) {
            c_env.reserve(proc.env->size() + 1);
            for (const auto &env : *proc.env) {
                c_env.push_back(env.c_str());
            }
        }
        c_env.push_back(nullptr);

        auto ret = ::chdir(proc.cwd.c_str());
        if (UNLIKELY(ret != 0)) {
            throw std::system_error(errno, std::system_category(), "chdir");
        }

        LINYAPS_BOX_LOG_DEBUG("exec command");

        child_chan.send_stage(protocol::stage::type::exec_ready);

        ::execvpe(c_args.at(0),
                  const_cast<char *const *>(c_args.data()),
                  const_cast<char *const *>(c_env.data()));

        throw std::system_error(errno, std::system_category(), "execvpe");
        // NOTE: Child process errors are intentionally logged and then swallowed here.
        // The parent process is NOT notified via a typed error message.  This is by
        // design: the child's error boundary is isolated from the parent so that
        // child failures cannot propagate as C++ exceptions into the parent's
        // control flow.  The parent detects the failure via the socket close and
        // the presence of forwarded fatal log messages during wait_for_close().
    } catch (const std::exception &e) {
        LINYAPS_BOX_LOG_FATAL("child process error: {}", e.what());
    } catch (...) {
        LINYAPS_BOX_LOG_FATAL("child process error: unknown error");
    }

    _exit(EXIT_FAILURE);
}

auto exec_parent_process(protocol::parent_message_channel sync,
                         bool expect_console_fd,
                         std::optional<linyaps_box::infra::unix_socket> external_console_socket,
                         bool mirror_host_size) -> int
{
    assert(!external_console_socket || expect_console_fd);

    auto inc = sync.drain_logs();
    pid_t pid{ };
    std::visit(linyaps_box::utils::Overload{
                 [&](const protocol::msg::pid_report &p) {
                     pid = p.value;
                 },
                 [&](const auto &) {
                     throw std::runtime_error("expected pid_report during exec");
                 },
               },
               inc.body);

    auto handle = infra::process_handle::open(pid);
    if (UNLIKELY(!handle)) {
        throw std::system_error(handle.error(),
                                fmt::format("failed to open container process {}", pid));
    }

    linyaps_box::container_monitor monitor{ handle->pid() };
    monitor.enable_signal_forwarding();

    // Unblock the grandchild so it can proceed with terminal setup and exec.
    // In the future, cgroup/scheduler setup with the real pid goes here,
    // between pid_report and proceed.
    sync.send_proceed();

    auto in = linyaps_box::utils::file_descriptor{ STDIN_FILENO, false };
    auto out = linyaps_box::utils::file_descriptor{ STDOUT_FILENO, false };

    bool changed{ false };
    auto in_flags = in.flags();
    auto out_flags = out.flags();

    auto restore_if_changed = linyaps_box::utils::make_defer([&]() noexcept {
        if (!changed) {
            return;
        }

        try {
            in.set_flags(in_flags);
            out.set_flags(out_flags);
        } catch (const std::exception &e) {
            LINYAPS_BOX_LOG_ERROR(
              "failed to restore stdin/stdout flags, some behavior may be unexpected: {}",
              e.what());
        }
    });

    try {
        if (expect_console_fd) {
            auto console_inc = sync.drain_logs();
            std::visit(linyaps_box::utils::Overload{
                         [&](const protocol::msg::console_fd &) {
                             auto fds = console_inc.take_fds();
                             auto master_fd = std::move(fds.front());

                             if (external_console_socket) {
                                 os::throw_if_error(
                                   external_console_socket->send_fd(master_fd.ref()));
                             } else {
                                 in.set_nonblock(true);
                                 out.set_nonblock(true);
                                 changed = true;

                                 monitor.enable_io_forwarding(
                                   linyaps_box::terminal_master{ std::move(master_fd) },
                                   in,
                                   out,
                                   mirror_host_size);
                             }
                         },
                         [&](const auto &) {
                             throw std::runtime_error("expected console_fd during exec");
                         },
                       },
                       console_inc.body);
        }

        sync.wait_for_stage(protocol::stage::type::exec_ready);
        sync.wait_for_close();

        return monitor.wait_container_exit();
    } catch (...) {
        monitor.kill_child();
        throw;
    }
}

} // anonymous namespace

container_ref::container_ref(status_directory status_dir, std::string id)
    : id_(std::move(id))
    , status_dir_(std::move(status_dir))
{
}

container_ref::~container_ref() noexcept = default;

container_status linyaps_box::container_ref::status() const
{
    return status_dir_.read();
}

namespace {

// the handle pins the process, but confirming that the pinned process
// is still the original container init is an operation-semantics check
// (pessimistic — refuse on uncertainty).
auto verify_container_process(const infra::process_handle &handle,
                              const container_status &st,
                              std::string_view action) -> void
{
    auto stat = handle.status();
    if (UNLIKELY(!stat)) {
        throw std::system_error(stat.error(),
                                fmt::format("cannot read status of container process {} "
                                            "before {}",
                                            st.pid,
                                            action));
    }

    if (UNLIKELY(stat->state == infra::process_state::zombie
                 || stat->state == infra::process_state::dead)) {
        throw std::system_error(std::make_error_code(std::errc::no_such_process),
                                fmt::format("container process {} is not running", st.pid));
    }

    if (!st.process_start_time) {
        LINYAPS_BOX_LOG_WARN("Legacy state files do not store the process {} startup time, so "
                             "the process identity cannot be verified at this time. We assume that "
                             "the current process is a container process.",
                             st.pid);
        return;
    }

    if (UNLIKELY(stat->start_time != *st.process_start_time)) {
        throw std::system_error(std::make_error_code(std::errc::no_such_process),
                                fmt::format("container PID {} was reused by another process; "
                                            "refusing to {}",
                                            st.pid,
                                            action));
    }
}

} // anonymous namespace

void container_ref::kill(int signal) const
{
    auto st = this->status();

    auto handle = infra::process_handle::open(st.pid);
    if (UNLIKELY(!handle)) {
        throw std::system_error(handle.error(),
                                fmt::format("failed to open container process {}", st.pid));
    }

    verify_container_process(*handle, st, "send signal");

    os::throw_if_error(handle->send_signal(signal),
                       fmt::format("failed to kill container process {}", st.pid));
}

auto container_ref::exec(exec_container_option option) const -> int
{
    auto st = this->status();

    // Defense against PID reuse
    auto handle = infra::process_handle::open(st.pid);
    if (UNLIKELY(!handle)) {
        throw std::system_error(handle.error(),
                                fmt::format("failed to open container process {}", st.pid));
    }

    verify_container_process(*handle, st, "exec process");

    auto target_pid = handle->pid();

    os::throw_if_error(os::set_child_subreaper(true));

    auto config = config::oci_config::parse(status_dir_.config());
    auto &proc = resolve_final_process(option, config);

    config::validate(proc);

    auto [parent_chan, child_chan] = protocol::create_message_socketpair();

    auto child = ::fork();
    if (UNLIKELY(child < 0)) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (child == 0) {
        parent_chan.close();
        option.console_socket.reset();

        exec_child_process(target_pid, config, proc, option.preserve_fds, std::move(child_chan));
    }

    child_chan.close();
    return exec_parent_process(std::move(parent_chan),
                               proc.terminal.value_or(false),
                               std::move(option.console_socket),
                               !proc.console_size_.has_value());
}

const status_directory &container_ref::status_dir() const
{
    return status_dir_;
}

const std::string &container_ref::get_id() const
{
    return id_;
}

} // namespace linyaps_box
