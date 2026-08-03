// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/container_monitor.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/process.h"
#include "linyaps_box/protocol/message_channel.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/close_range.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/utils/session.h"
#include "linyaps_box/utils/setns.h"
#include "linyaps_box/utils/utils.h"

#include <algorithm>
#include <cassert>
#include <csignal> // IWYU pragma: keep
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include <grp.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linyaps_box {

namespace {

auto resolve_final_process(linyaps_box::exec_container_option &option,
                           const linyaps_box::oci_config &config)
  -> linyaps_box::oci_config::process_t &
{
    if (!option.proc) {
        option.proc = config.process ? *config.process : linyaps_box::oci_config::process_t{ };
    }

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

    if (!option.command.empty()) {
        proc.args = std::move(option.command);
    }

    if (option.uid) {
        proc.user.uid = *option.uid;
    }

    if (option.gid) {
        proc.user.gid = *option.gid;
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (option.caps) {
        if (!proc.capabilities) {
            proc.capabilities.emplace();
        }

        proc.capabilities->effective = *option.caps;
        proc.capabilities->ambient = *option.caps;
        proc.capabilities->bounding = *option.caps;
        proc.capabilities->permitted = *option.caps;
    }
#endif

    return proc;
}

void child_setup_terminal(const linyaps_box::oci_config::process_t &proc,
                          protocol::child_message_channel &sync)
{
    if (!proc.terminal) {
        return;
    }

    auto [master, slave] = linyaps_box::create_pty_pair();

    slave.setup_stdio();
    if (proc.console_size) {
        slave.set_size({ proc.console_size->height, proc.console_size->width, 0, 0 });
    }

    auto console_fd = std::move(master).take();
    auto ref = console_fd.ref();
    sync.send(protocol::msg::console_fd{ },
              linyaps_box::utils::span<const linyaps_box::utils::file_descriptor_ref>{ &ref, 1 });
}

void child_apply_credentials(const linyaps_box::oci_config::process_t &proc)
{
    if (proc.user.umask) {
        ::umask(*proc.user.umask);
    }

    if (proc.user.uid == 0 && proc.user.gid == 0 && !proc.user.additional_gids) {
        return;
    }

    if (::setresgid(proc.user.gid, proc.user.gid, proc.user.gid) != 0) {
        _exit(EXIT_FAILURE);
    }

    if (proc.user.additional_gids) {
        if (::setgroups(proc.user.additional_gids->size(), proc.user.additional_gids->data())
            != 0) {
            _exit(EXIT_FAILURE);
        }
    }

    if (::setresuid(proc.user.uid, proc.user.uid, proc.user.uid) != 0) {
        _exit(EXIT_FAILURE);
    }

    for (auto fd : { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO }) {
        if (::fchown(fd, proc.user.uid, proc.user.gid) != 0) {
            if (errno != EINVAL && errno != ENOSYS) {
                _exit(EXIT_FAILURE);
            }
        }
    }
}

void child_apply_environment(const linyaps_box::oci_config::process_t &proc,
                             const linyaps_box::oci_config &config)
{
    ::clearenv();

    if (config.process) {
        for (const auto &env : *config.process->env) {
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

void child_apply_rlimits(const linyaps_box::oci_config::process_t &proc)
{
    if (!proc.rlimits) {
        return;
    }

    for (const auto &rl : *proc.rlimits) {
        auto resource = static_cast<int>(rl.type);
        const struct rlimit limit{ rl.soft, rl.hard };
        if (::setrlimit(resource, &limit) != 0) {
            _exit(EXIT_FAILURE);
        }
    }
}

#ifdef LINYAPS_BOX_ENABLE_CAP
void child_apply_capabilities(const linyaps_box::oci_config::process_t &proc,
                              const linyaps_box::oci_config &config)
{
    LINYAPS_BOX_LOG_DEBUG("apply capabilities");
    // Determine which caps to apply: prefer the exec-specific set, fall back to
    // the container's config.json default if the exec set is entirely empty.
    auto effective_caps =
      [&]() -> std::optional<linyaps_box::oci_config::process_t::capabilities_t> {
        if (!proc.capabilities) {
            return config.process->capabilities;
        }

        auto all_empty = [](const linyaps_box::oci_config::process_t::capabilities_t &c) {
            return (!c.effective || c.effective->empty()) && (!c.bounding || c.bounding->empty())
              && (!c.inheritable || c.inheritable->empty())
              && (!c.permitted || c.permitted->empty()) && (!c.ambient || c.ambient->empty());
        };

        if (all_empty(*proc.capabilities)) {
            return config.process->capabilities;
        }

        return proc.capabilities;
    }();

    if (!effective_caps) {
        return;
    }

    auto should_apply = [](const linyaps_box::oci_config::process_t::capabilities_t &c) {
        return (c.effective && !c.effective->empty()) || (c.bounding && !c.bounding->empty())
          || (c.inheritable && !c.inheritable->empty()) || (c.permitted && !c.permitted->empty())
          || (c.ambient && !c.ambient->empty());
    };

    if (!should_apply(*effective_caps)) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Set capabilities for exec");

    // Drop every cap not in the bounding set.
    if (effective_caps->bounding) {
        const auto &bounding_set = *effective_caps->bounding;
        std::ifstream cap_file("/proc/sys/kernel/cap_last_cap");
        unsigned long last_cap{ 0 };
        cap_file >> last_cap;
        for (unsigned long cap = 0; cap < last_cap; ++cap) {
            if (std::find(bounding_set.cbegin(), bounding_set.cend(), static_cast<int>(cap))
                == bounding_set.cend()) {
                if (cap_drop_bound(static_cast<int>(cap)) < 0) {
                    throw std::system_error(errno, std::system_category(), "cap_drop_bound");
                }
            }
        }
    }

    auto *cap = cap_init();
    if (cap == nullptr) {
        throw std::system_error(errno, std::system_category(), "cap_init");
    }
    const std::unique_ptr<_cap_struct, decltype(&cap_free)> caps(cap, cap_free);

    auto set_cap_flag = [&caps](const std::vector<cap_value_t> &cap_set, cap_flag_t flag) {
        if (cap_set.empty()) {
            return;
        }
        auto ret = cap_set_flag(caps.get(), flag, cap_set.size(), cap_set.data(), CAP_SET);
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "cap_set_flag");
        }
    };

    if (effective_caps->effective) {
        set_cap_flag(*effective_caps->effective, CAP_EFFECTIVE);
    }

    if (effective_caps->permitted) {
        set_cap_flag(*effective_caps->permitted, CAP_PERMITTED);
    }

    if (effective_caps->inheritable) {
        set_cap_flag(*effective_caps->inheritable, CAP_INHERITABLE);
    }

    if (cap_set_proc(caps.get()) < 0) {
        throw std::system_error(errno, std::system_category(), "cap_set_proc");
    }

    os::throw_if_error(os::prctl(PR_SET_KEEPCAPS, 1L, 0L, 0L, 0L));

    if (cap_set_proc(caps.get()) < 0) {
        throw std::system_error(errno, std::system_category(), "cap_set_proc");
    }

#  ifdef PR_CAP_AMBIENT
    os::throw_if_error(os::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0L, 0L, 0L));
    if (const auto &ambient_set = effective_caps->ambient; ambient_set) {
        std::for_each(ambient_set->cbegin(), ambient_set->cend(), [](cap_value_t cap) {
            os::throw_if_error(os::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0L, 0L));
        });
    }
#  endif
}
#endif // LINYAPS_BOX_ENABLE_CAP

[[noreturn]] auto exec_child_process(pid_t target_pid,
                                     const linyaps_box::oci_config &config,
                                     const linyaps_box::oci_config::process_t &proc,
                                     int preserve_fds,
                                     protocol::child_message_channel child_chan) -> void
{
    try {
        auto &logger = linyaps_box::log::global_logger::instance();
        logger.unset_sink();
        logger.set_sink(linyaps_box::log::sync_socket_sink(child_chan));

        bool pid_ns{ false };
        if (config.linux && config.linux->namespaces) {
            linyaps_box::utils::join_container_namespaces(target_pid, *config.linux);
            pid_ns = std::any_of(config.linux->namespaces->cbegin(),
                                 config.linux->namespaces->cend(),
                                 [](const auto &ns) {
                                     return ns.type_
                                       == linyaps_box::oci_config::linux_t::namespace_t::type::PID;
                                 });
        }

        if (pid_ns) {
            auto grandchild = ::fork();
            if (UNLIKELY(grandchild < 0)) {
                throw std::system_error(errno, std::system_category(), "fork grandchild failed");
            }

            if (grandchild > 0) {
                child_chan.send(protocol::msg::pid_report{ static_cast<pid_t>(grandchild) });
                _exit(EXIT_SUCCESS);
            }
        } else {
            child_chan.send(protocol::msg::pid_report{ ::getpid() });
        }

        child_chan.wait_for_proceed();

        linyaps_box::utils::setsid();

        if (proc.terminal) {
            child_setup_terminal(proc, child_chan);
        }

        child_apply_environment(proc, config);

        child_apply_rlimits(proc);

        linyaps_box::utils::close_range(3U + static_cast<unsigned>(preserve_fds),
                                        std::numeric_limits<unsigned>::max(),
                                        CLOSE_RANGE_CLOEXEC);

        child_apply_credentials(proc);

        if (proc.no_new_privileges) {
            LINYAPS_BOX_LOG_DEBUG("Set no new privileges");
            os::throw_if_error(os::prctl(PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L));
        }

#ifdef LINYAPS_BOX_ENABLE_CAP
        child_apply_capabilities(proc, config);
#endif

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
                         std::optional<linyaps_box::infra::unix_socket> external_console_socket)
  -> int
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

    linyaps_box::container_monitor monitor{ pid };
    monitor.enable_signal_forwarding();

    // Unblock the grandchild so it can proceed with terminal setup and exec.
    // In the future, cgroup/scheduler setup with the real pid goes here,
    // between pid_report and proceed.
    sync.send(protocol::msg::proceed{ });

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
                                   out);
                             }
                         },
                         [&](const auto &) {
                             throw std::runtime_error("expected console_fd during exec");
                         },
                       },
                       console_inc.body);
        }

        sync.wait_for(protocol::stage::type::exec_ready);
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

container_status_t linyaps_box::container_ref::status() const
{
    return status_dir_.read();
}

void container_ref::kill(int signal) const
{
    auto pid = this->status().PID;

    LINYAPS_BOX_LOG_DEBUG("kill process {} with signal {}", pid, signal);
    if (::kill(pid, signal) == 0) {
        return;
    }

    std::stringstream ss;
    ss << "failed to kill process " << pid << " with signal " << signal;
    throw std::system_error(errno, std::system_category(), std::move(ss).str());
}

auto container_ref::exec(exec_container_option option) const -> int
{
    auto target_pid = this->status().PID;

    os::throw_if_error(os::prctl(PR_SET_CHILD_SUBREAPER, 1L, 0L, 0L, 0L));

    auto config = oci_config::parse(status_dir_.config());
    auto &proc = resolve_final_process(option, config);

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
                               std::move(option.console_socket));
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
