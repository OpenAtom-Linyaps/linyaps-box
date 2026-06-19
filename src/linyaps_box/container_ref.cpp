// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_ref.h"

#include "linyaps_box/container_monitor.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/close_range.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/utils/process.h"
#include "linyaps_box/utils/session.h"
#include "linyaps_box/utils/setns.h"

#include <algorithm>
#include <csignal> // IWYU pragma: keep
#include <cstdint>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include <grp.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

enum class SyncType : uint8_t {
    GrandchildPid,
    ConsoleFd,
    Proceed,
};

struct SyncMessage
{
    SyncType type;
    pid_t pid;
};

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

auto has_pid_namespace(const linyaps_box::oci_config &config) -> bool
{
    if (!config.linux || !config.linux->namespaces) {
        return false;
    }

    return std::any_of(config.linux->namespaces->cbegin(),
                       config.linux->namespaces->cend(),
                       [](const auto &ns) {
                           return ns.type_
                             == linyaps_box::oci_config::linux_t::namespace_t::type::PID;
                       });
}

void child_setup_terminal(const linyaps_box::oci_config::process_t &proc,
                          linyaps_box::unix_socket &child_sock,
                          const linyaps_box::exec_container_option &option)
{
    if (!proc.terminal) {
        return;
    }

    auto [master, slave] = linyaps_box::create_pty_pair();

    slave.setup_stdio();
    if (proc.console_size) {
        slave.set_size({ proc.console_size->height, proc.console_size->width, 0, 0 });
    }

    if (option.console_socket) {
        option.console_socket->send_fd(std::move(master).take());
    } else {
        child_sock.send_msg_with_fd(std::move(master).take(),
                                    SyncMessage{ SyncType::ConsoleFd, 0 });
    }
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

    LINYAPS_BOX_DEBUG() << "Set capabilities for exec";

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

    std::ignore = linyaps_box::utils::prctl(PR_SET_KEEPCAPS, 1);

    if (cap_set_proc(caps.get()) < 0) {
        throw std::system_error(errno, std::system_category(), "cap_set_proc");
    }

    std::ignore = linyaps_box::utils::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0L, 0L, 0L);
    if (effective_caps->ambient) {
        for (const auto &ambient : *effective_caps->ambient) {
            std::ignore =
              linyaps_box::utils::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, ambient, 0L, 0L);
        }
    }
}
#endif // LINYAPS_BOX_ENABLE_CAP

[[noreturn]] void exec_child_process(pid_t target_pid,
                                     const linyaps_box::oci_config &config,
                                     const linyaps_box::oci_config::process_t &proc,
                                     const linyaps_box::exec_container_option &option,
                                     linyaps_box::unix_socket child_sock)
try {
    bool pid_ns{ false };
    if (config.linux && config.linux->namespaces) {
        linyaps_box::utils::join_container_namespaces(target_pid, *config.linux);
        pid_ns = has_pid_namespace(config);
    }

    // Double-fork for PID namespace
    if (pid_ns) {
        auto grandchild = ::fork();
        if (grandchild < 0) {
            _exit(EXIT_FAILURE);
        }

        if (grandchild > 0) {
            child_sock.send_msg(SyncMessage{ SyncType::GrandchildPid, grandchild });
            _exit(EXIT_SUCCESS);
        }
    } else {
        child_sock.send_msg(SyncMessage{ SyncType::GrandchildPid, ::getpid() });
    }

    linyaps_box::utils::setsid();

    child_setup_terminal(proc, child_sock, option);

    // Sync socket is no longer needed — close before fd cleanup and exec.
    child_sock.close();

    child_apply_environment(proc, config);

    child_apply_rlimits(proc);

    linyaps_box::utils::close_range(3U + static_cast<unsigned>(option.preserve_fds),
                                    std::numeric_limits<unsigned>::max(),
                                    CLOSE_RANGE_CLOEXEC);

    child_apply_credentials(proc);

    if (proc.no_new_privileges) {
        LINYAPS_BOX_DEBUG() << "Set no new privileges";
        std::ignore = linyaps_box::utils::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
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
    if (ret != 0) {
        throw std::system_error(errno, std::system_category(), "chdir");
    }

    ::execvpe(c_args.at(0),
              const_cast<char *const *>(c_args.data()),
              const_cast<char *const *>(c_env.data()));

    throw std::system_error(errno, std::system_category(), "execvpe");
} catch (const std::exception &e) {
    LINYAPS_BOX_ERR() << "child process fatal error: " << e.what();
    _exit(EXIT_FAILURE);
}

auto exec_parent_process(linyaps_box::unix_socket &parent_sock,
                         const linyaps_box::oci_config::process_t &proc,
                         const linyaps_box::exec_container_option &option) -> int
{
    auto pid_msg = parent_sock.recv_msg<SyncMessage>();
    if (pid_msg.type != SyncType::GrandchildPid) {
        throw std::runtime_error("unexpected sync message type");
    }

    linyaps_box::container_monitor monitor{ pid_msg.pid };
    monitor.enable_signal_forwarding();

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
            LINYAPS_BOX_ERR()
              << "failed to restore stdin/stdout flags, some behavior may be unexpected: "
              << e.what();
        }
    });

    if (proc.terminal && !option.console_socket) {
        auto [msg, master_fd] = parent_sock.recv_msg_with_fd<SyncMessage>();
        if (msg.type != SyncType::ConsoleFd) {
            throw std::runtime_error("unexpected sync message type");
        }

        in.set_nonblock(true);
        out.set_nonblock(true);
        changed = true;

        monitor.enable_io_forwarding(linyaps_box::terminal_master{ std::move(master_fd) }, in, out);
    }

    parent_sock.close();

    return monitor.wait_container_exit();
}

} // anonymous namespace

namespace linyaps_box {

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

    LINYAPS_BOX_DEBUG() << "kill process " << pid << " with signal " << signal;
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

    std::ignore = utils::prctl(PR_SET_CHILD_SUBREAPER, 1L, 0L, 0L, 0L);

    auto config = oci_config::parse(status_dir_.config());
    auto &proc = resolve_final_process(option, config);

    auto [parent_sock, child_sock] = unix_socket::pair();

    auto child = ::fork();
    if (UNLIKELY(child < 0)) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (child == 0) {
        parent_sock.close();
        exec_child_process(target_pid, config, proc, option, std::move(child_sock));
    }

    child_sock.close();

    return exec_parent_process(parent_sock, proc, option);
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
