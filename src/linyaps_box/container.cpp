// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container.h"

#include "linyaps_box/config/mount_options.h"
#include "linyaps_box/container_monitor.h"
#include "linyaps_box/impl/disabled_cgroup_manager.h"
#include "linyaps_box/infra/process_handle.h"
#include "linyaps_box/infra/rootfs.h"
#include "linyaps_box/infra/unix_socket.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/process.h"
#include "linyaps_box/os/system.h"
#include "linyaps_box/protocol/message_channel.h"
#include "linyaps_box/protocol/sync_socket_forwarder.h"
#include "linyaps_box/security/privilege.h"
#include "linyaps_box/terminal.h"
#include "linyaps_box/utils/cgroups.h"
#include "linyaps_box/utils/close_range.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/session.h"
#include "linyaps_box/utils/signal.h"
#include "utils/defer.h"

#include <linux/magic.h>
#include <linux/sched.h>
#include <nlohmann/json.hpp>
#include <sys/mount.h>
#include <sys/signalfd.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

#include <grp.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

using namespace linyaps_box;

namespace {

using config::mount_options::dump;
using protocol::child_message_channel;
using protocol::parent_message_channel;
namespace stage = protocol::stage;

[[maybe_unused]] auto get_pid_namespace() -> std::string
{
    auto result =
      os::throw_if_error(os::readlinkat(utils::file_descriptor_ref::cwd(), "/proc/self/ns/pid"));
    const std::string_view pid_ns = result.native();

    constexpr std::string_view prefix = "pid:[";
    constexpr char suffix = ']';
    constexpr auto prefix_len = prefix.size();
    constexpr auto total_wrapper_len = prefix_len + 1;

    if (pid_ns.size() < total_wrapper_len) {
        return "invalid format";
    }

    if (pid_ns.rfind(prefix, 0) != 0) {
        return "invalid format";
    }

    if (pid_ns.back() != suffix) {
        return "invalid format";
    }

    return std::string{ pid_ns.substr(prefix_len, pid_ns.size() - total_wrapper_len) };
}

void execute_hook(const oci_config::hooks_t::hook_t &hook, const container_status &state)
{
    // FIXME: hook state JSON is sent over a SEQPACKET socketpair, which discards
    //  bytes beyond the hook's first read() buffer (e.g. Python's 8K) when the
    //  message is larger — large `annotations` can truncate the JSON and make the
    //  hook fail to parse its stdin.  runc, youki and crun all feed hook stdin via
    //  a stream pipe (pipe2(O_CLOEXEC) + write_all) instead; replace this with
    //  pipe2 + file_descriptor::write_span in a follow-up.
    auto [parent, child] = infra::unix_socket::create_pair(os::sys::socket_type::seqpacket,
                                                           os::sys::socket_flag::cloexec);

    auto pid = fork();
    if (pid < 0) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (pid == 0) {
        child.close();
        parent.fd().duplicate_to(STDIN_FILENO, 0);
        parent.close();

        const auto *bin = hook.path.c_str();
        std::vector<const char *> c_args;
        if (hook.args) {
            const auto &args = hook.args.value();
            c_args.reserve(args.size() + 1);
            for (const auto &arg : args) {
                c_args.push_back(arg.c_str());
            }
        }
        c_args.push_back(nullptr);

        std::vector<const char *> c_env;
        if (hook.env) {
            for (const auto &env : hook.env.value()) {
                c_env.push_back(env.c_str());
            }
        }
        c_env.push_back(nullptr);

        execvpe(bin,
                const_cast<char *const *>(c_args.data()), // NOLINT
                const_cast<char *const *>(c_env.data())); // NOLINT

        LINYAPS_BOX_LOG_ERROR_ERRNO(errno,
                                    "execute hook {} failed",
                                    [&bin, &c_args]() -> std::string {
                                        std::stringstream stream;
                                        stream << bin;
                                        for (const auto &arg : c_args) {
                                            if (arg != nullptr) {
                                                stream << " " << arg;
                                            }
                                        }
                                        return std::move(stream).str();
                                    }());
        _exit(EXIT_FAILURE);
    }
    parent.close();

    auto state_json = nlohmann::json(state).dump();
    const auto *data = reinterpret_cast<const std::byte *>(state_json.data());
    auto remaining = state_json.size();

    if (auto r = child.send(utils::span(data, remaining)); !r) {
        LINYAPS_BOX_LOG_WARN("failed to write state to hook stdin: {}", r.error().message());
    }

    child.close();

    int status{ 0 };
    if (!hook.timeout) {
        pid_t ret = -1;
        while (ret == -1) {
            ret = waitpid(pid, &status, 0);
            if (ret != -1) {
                break;
            }

            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }

            throw std::system_error(errno,
                                    std::system_category(),
                                    "waitpid " + std::to_string(pid));
        }
    } else {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);

        struct timespec ts{ };
        ts.tv_sec = hook.timeout.value();

        siginfo_t info;
        while (true) {
            auto sig = sigtimedwait(&mask, &info, &ts);
            if (sig >= 0) {
                if (info.si_pid != pid) {
                    continue;
                }

                auto ret = waitpid(pid, &status, 0);
                if (ret < 0) {
                    throw std::system_error(errno,
                                            std::system_category(),
                                            "waitpid " + std::to_string(pid));
                }

                break;
            }

            if (errno == EAGAIN) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(errno, std::system_category(), "sigtimedwait");
        }
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            throw std::runtime_error("hook " + hook.path.string() + " failed with exit code "
                                     + std::to_string(WEXITSTATUS(status)));
        }

        return;
    }

    if (WIFSIGNALED(status)) {
        throw std::runtime_error("hook " + hook.path.string() + " terminated by signal "
                                 + std::to_string(WTERMSIG(status)));
    }
}

struct clone_fn_args
{
    int preserve_fds;
    linyaps_box::container *container{ nullptr };
    child_message_channel sync;
};

// NOTE: All function in this namespace are running in the container namespace.
namespace container_ns {

void initialize_container(const oci_config &oci_config, child_message_channel &sync)
{
    LINYAPS_BOX_LOG_DEBUG("Request OCI runtime in runtime namespace to configure namespace");

    sync.send_stage(stage::type::namespace_ready);
    sync.expect_stage(stage::type::namespace_done);

    LINYAPS_BOX_LOG_DEBUG("Container namespaces configured from runtime namespace");

    const auto &linux = oci_config.linux;
    const auto has_uts_namespace = linux && linux->namespaces
      && std::any_of(linux->namespaces->cbegin(),
                     linux->namespaces->cend(),
                     [](const oci_config::linux_t::namespace_t &ns) {
                         return ns.type_ == oci_config::linux_t::namespace_t::type::UTS;
                     });

    if (oci_config.hostname) {
        if (UNLIKELY(!has_uts_namespace)) {
            throw std::runtime_error("hostname requires the UTS namespace");
        }

        LINYAPS_BOX_LOG_DEBUG("Set container hostname to {}", oci_config.hostname.value());
        os::throw_if_error(os::sethostname(oci_config.hostname.value()));
    }

    if (oci_config.domainname) {
        if (UNLIKELY(!has_uts_namespace)) {
            throw std::runtime_error("domainname requires the UTS namespace");
        }

        LINYAPS_BOX_LOG_DEBUG("Set container domainname to {}", oci_config.domainname.value());
        os::throw_if_error(os::setdomainname(oci_config.domainname.value()));
    }

    if (oci_config.process->oom_score_adj) {
        auto score = std::to_string(oci_config.process->oom_score_adj.value());
        LINYAPS_BOX_LOG_DEBUG("Set oom score to {}", score);

        std::ofstream ofs("/proc/self/oom_score_adj");
        if (!ofs) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "failed to open /proc/self/oom_score_adj");
        }

        ofs << score;
        if (!ofs) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "failed to write to /proc/self/oom_score_adj");
        }
    }
}

void syscall_mount(const char *_special_file,
                   const char *_dir,
                   const char *_fstype,
                   unsigned long int _rwflag,
                   const void *_data)
{
    LINYAPS_BOX_LOG_DEBUG(
      "mount\n\t_special_file = {}\n\t_dir = {}\n\t_fstype = {}\n\t_rwflag = {}\n\t_data = {}",
      [_special_file]() -> std::string {
          if (_special_file == nullptr) {
              return "nullptr";
          }

          return _special_file;
      }(),
      [_dir]() -> std::string {
          if (_dir == nullptr) {
              return "nullptr";
          }

          return _dir;
      }(),
      [_fstype]() -> std::string {
          if (_fstype == nullptr) {
              return "nullptr";
          }
          return _fstype;
      }(),
      dump(_rwflag),
      [_data]() -> std::string {
          if (_data == nullptr) {
              return "nullptr";
          }
          return static_cast<const char *>(_data);
      }());

    LINYAPS_BOX_LOG_DEBUG("mount called: source={}, dest={}, type={}, flags={:#x}",
                          _special_file ? _special_file : "(null)",
                          _dir ? _dir : "(null)",
                          _fstype ? _fstype : "(null)",
                          _rwflag);
    auto ret = ::mount(_special_file, _dir, _fstype, _rwflag, _data);
    if (ret < 0) {
        LINYAPS_BOX_LOG_DEBUG("mount failed: {} (errno={})", std::strerror(errno), errno);
        throw std::system_error(errno, std::system_category(), "mount");
    }
}

struct remount_t
{
    utils::file_descriptor destination_fd;
    unsigned long flags{ };
    std::string data;
};

auto do_remount(const remount_t &mount) -> void
{
    if (UNLIKELY(mount.destination_fd.get() == -1)) {
        throw std::invalid_argument("remount: invalid destination file descriptor");
    }

    if (UNLIKELY((mount.flags & (MS_BIND | MS_REMOUNT | MS_RDONLY)) == 0)) {
        throw std::invalid_argument("remount: flags must include BIND|REMOUNT|RDONLY");
    }

    auto destination = mount.destination_fd.ref().current_path();
    const auto *data_ptr = mount.data.empty() ? nullptr : mount.data.c_str();

    // for old kernel
    if ((mount.flags & (MS_REMOUNT | MS_RDONLY)) != 0) {
        data_ptr = nullptr;
    }

    LINYAPS_BOX_LOG_DEBUG("Remount {} with flags {}", destination, dump(mount.flags));
    try {
        syscall_mount(nullptr, destination.c_str(), nullptr, mount.flags, data_ptr);
        return;
    } catch (const std::system_error &e) {
        LINYAPS_BOX_LOG_DEBUG("Failed to remount {} with flags {}: {}, retrying",
                              mount.destination_fd.get(),
                              dump(mount.flags),
                              e.what());
    }

    auto state = os::throw_if_error(os::fstatfs(mount.destination_fd.ref()));
    const auto dest_flag = static_cast<unsigned long>(state.f_flags);

    auto remount_flags = dest_flag & (MS_NOSUID | MS_NODEV | MS_NOEXEC);
    if ((remount_flags | mount.flags) != mount.flags) {
        try {
            syscall_mount(nullptr,
                          destination.c_str(),
                          nullptr,
                          remount_flags | mount.flags,
                          data_ptr);
            return;
        } catch (const std::system_error &e) {
            LINYAPS_BOX_LOG_DEBUG("Failed to remount {} with flags {}: {}, retrying",
                                  mount.destination_fd.get(),
                                  dump(remount_flags | mount.flags),
                                  e.what());
        }

        if ((dest_flag & MS_RDONLY) != 0) {
            remount_flags = dest_flag & (MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY);
            syscall_mount(nullptr,
                          destination.c_str(),
                          nullptr,
                          mount.flags | remount_flags,
                          data_ptr);
            return;
        }
    }

    throw std::runtime_error("remount failed after all fallbacks");
}

[[nodiscard]] utils::file_descriptor ensure_mount_destination(const infra::Root &root,
                                                              const oci_config::mount_t &mount,
                                                              bool isDir = false)
{
    LINYAPS_BOX_LOG_DEBUG("Opening {} {} under {}",
                          (isDir ? "directory " : "file "),
                          mount.destination,
                          root.ref().current_path());

    auto ret =
      root.open(mount.destination, { os::sys::open_flag::cloexec, os::sys::access_mode::path });
    if (UNLIKELY(!ret)) {
        const auto &err = ret.error();
        if (err != std::errc::no_such_file_or_directory) {
            throw std::system_error(
              err,
              fmt::format("failed to open destination {}", mount.destination));
        }

        const auto &path = mount.destination;

        // NOTE: Automatically create destination is not a part of the OCI runtime
        // spec, as it requires implementation to follow the behavior of mount.
        // But both crun and runc does this.

        if (isDir) {
            return os::throw_if_error(root.create_directories(path));
        }

        if (path.has_parent_path()) {
            auto parent = os::throw_if_error(root.create_directories(path.parent_path()));
            return os::throw_if_error(
              os::openat(parent.ref(),
                         path.filename(),
                         { os::sys::open_flag::create | os::sys::open_flag::exclusive
                             | os::sys::open_flag::no_follow,
                           os::sys::access_mode::read_write },
                         os::default_new_file_perm));
        }

        return os::throw_if_error(
          os::openat(root.ref(),
                     path.filename(),
                     { os::sys::open_flag::create | os::sys::open_flag::exclusive
                         | os::sys::open_flag::no_follow,
                       os::sys::access_mode::read_write },
                     os::default_new_file_perm));
    }

    return std::move(*ret);
}

auto do_propagation_mount(const utils::file_descriptor &destination, unsigned long flags) -> void
{
    LINYAPS_BOX_LOG_DEBUG("mount propagation flags");

    if (flags == 0) {
        return;
    }

    if (UNLIKELY(!destination.valid())) {
        throw std::invalid_argument("invalid destination file descriptor for propagation mount");
    }

    auto dest_path = destination.ref().current_path();
    if (dest_path.empty()) {
        return;
    }

    syscall_mount(nullptr, dest_path.c_str(), nullptr, flags, nullptr);
}

[[nodiscard]] utils::file_descriptor do_bind_mount(const infra::Root &root,
                                                   const oci_config::mount_t &mount)
{
    if (UNLIKELY(!mount.source)) {
        throw std::invalid_argument("bind mount requires source");
    }

    auto source_fd = os::throw_if_error(
      os::open(mount.source.value(), { os::sys::open_flag::cloexec, os::sys::access_mode::path }));
    auto source_ref = source_fd.ref();
    auto source_stat =
      os::throw_if_error(os::fstatat(source_ref, "", os::sys::at_flag::empty_path));

    // TODO: if we do a bind mount after pivot_root and this container doesn't mount a procfs,
    // we need try a mount directly.
    auto sourceIsDir = S_ISDIR(source_stat.st_mode);
    auto destination_fd = ensure_mount_destination(root, mount, sourceIsDir);
    auto dest_stat =
      os::throw_if_error(os::fstatat(destination_fd.ref(), "", os::sys::at_flag::empty_path));
    if (sourceIsDir != S_ISDIR(dest_stat.st_mode)) {
        throw std::invalid_argument(
          fmt::format("bind mount source/destination type mismatch: {} ({}) and {} ({})",
                      mount.source.value(),
                      (sourceIsDir ? "dir" : "file"),
                      mount.destination,
                      (S_ISDIR(dest_stat.st_mode) ? "dir" : "file")));
    }

    // remove MS_RDONLY for creating destination
    // we will remount it on later
    auto bind_flags = mount.vfs_flags & ~MS_RDONLY;
    try {
        // bind mount will ignore fstype and data
        syscall_mount(source_ref.proc_path().c_str(),
                      destination_fd.ref().proc_path().c_str(),
                      nullptr,
                      bind_flags,
                      nullptr);
    } catch ([[maybe_unused]] const std::system_error &e) {
        throw;
    }

    return os::throw_if_error(
      root.open(mount.destination, { os::sys::open_flag::cloexec, os::sys::access_mode::path }));
}

[[noreturn]] void do_cgroup_mount([[maybe_unused]] const infra::Root &root,
                                  [[maybe_unused]] const oci_config::mount_t &mount,
                                  [[maybe_unused]] std::string_view unified_cgroup_path)
{
    // TODO: implement full cgroup mount logic.
    throw std::runtime_error("mount cgroup: Not implemented");
}

[[nodiscard]] std::optional<remount_t> do_mount(container &container,
                                                infra::Root &root,
                                                const oci_config::mount_t &mount)
{
    LINYAPS_BOX_LOG_DEBUG(
      "Mount {} to {}",
      [&]() -> std::string {
          std::stringstream result;
          if (mount.type) {
              result << mount.type.value() << ":";
          }
          result << mount.source.value_or("none");
          return result.str();
      }(),
      mount.destination.string());

    // Cgroup mounts: skip if /sys/fs/cgroup is already visible via
    // a recursive bind mount of /sys, otherwise delegate to do_cgroup_mount.
    static bool is_sys_rbind{ false };
    if (mount.type && mount.type.value().rfind("cgroup", 0) != std::string::npos) {
        if (mount.destination == "/sys/fs/cgroup" && is_sys_rbind) {
            return std::nullopt;
        }
        do_cgroup_mount(root, mount, "");
        return std::nullopt;
    }

    if ((mount.vfs_flags & MS_BIND) != 0 && mount.destination == "/sys"
        && (mount.vfs_flags & MS_REC) != 0) {
        is_sys_rbind = true;
    }

    utils::file_descriptor destination_fd;
    if ((mount.vfs_flags & MS_BIND) != 0) {
        destination_fd = do_bind_mount(root, mount);

        if (mount.destination == "/dev") {
            container.set_mount_dev_from_host();
        }
    } else {
        // mount other types
        destination_fd = ensure_mount_destination(root, mount, true);
        try {
            syscall_mount(mount.source ? mount.source.value().c_str() : nullptr,
                          destination_fd.ref().proc_path().c_str(),
                          mount.type ? mount.type.value().c_str() : nullptr,
                          mount.vfs_flags,
                          mount.data.empty() ? nullptr : mount.data.c_str());
        } catch (const std::system_error &e) {
            // TODO: refactor below codes
            if (mount.type && *mount.type == "sysfs" && e.code().value() == EPERM) {
                const auto &linux = container.get_config().linux;
                if (linux && linux->uid_mappings && !linux->uid_mappings->empty()) {
                    LINYAPS_BOX_LOG_DEBUG("sysfs mount failed, fallback to bind mount /sys");
                    syscall_mount("/sys",
                                  destination_fd.ref().proc_path().c_str(),
                                  nullptr,
                                  MS_BIND | MS_REC,
                                  nullptr);
                    destination_fd = os::throw_if_error(
                      root.open(mount.destination,
                                { os::sys::open_flag::cloexec, os::sys::access_mode::path }));

                    // mask /sys/fs/cgroup to prevent host cgroup leakage unless explicitly mounted
                    auto has_cgroup_mount =
                      std::any_of(container.get_config().mounts.cbegin(),
                                  container.get_config().mounts.cend(),
                                  [](const auto &m) {
                                      return m.destination == "/sys/fs/cgroup";
                                  });

                    if (!has_cgroup_mount) {
                        LINYAPS_BOX_LOG_DEBUG("mask /sys/fs/cgroup");
                        auto cgroup_dest =
                          root.open("/sys/fs/cgroup",
                                    { os::sys::open_flag::cloexec, os::sys::access_mode::path });
                        if (cgroup_dest) {
                            syscall_mount("/dev/null",
                                          cgroup_dest->ref().proc_path().c_str(),
                                          nullptr,
                                          MS_BIND,
                                          nullptr);
                        } else {
                            LINYAPS_BOX_LOG_DEBUG("/sys/fs/cgroup not found ({}), skipping mask",
                                                  cgroup_dest.error().message());
                        }
                    }
                } else {
                    throw;
                }
            } else {
                throw;
            }
        }
    }

    if (auto prop_flags = mount.propagation_flags; prop_flags != 0) {
        do_propagation_mount(destination_fd, prop_flags);
    }

    bool need_remount{ false };
    // we will do all bind/ro mount at finalize so we can
    // create missing destinations
    if ((mount.vfs_flags & (MS_RDONLY | MS_BIND)) != 0) {
        need_remount = true;
    }

    // procfs mount option only working with remount (e.g. hidepid=2)
    // this limitation is no longer required after kernel 5.7
    // we do this for compatible with older kernel
    // https://web.git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=69879c01a0c3f70e0887cfb4d9ff439814361e46
    if (!mount.data.empty() && mount.type == "proc") {
        need_remount = true;
    }

    // if the mount destination is root, we need to reopen it after mount
    // to refresh the file descriptor, otherwise it may cause some unexpected behavior
    auto maybe_refresh_root = [&root, &mount] {
        if (mount.destination == "/") {
            os::throw_if_error(root.reopen());
        }
    };

    maybe_refresh_root();

    if (!need_remount) {
        LINYAPS_BOX_LOG_DEBUG("no need to remount");
        return std::nullopt;
    }

    auto remount_flags = mount.vfs_flags | MS_REMOUNT;
    if (mount.type != "proc") {
        // Linux kernel requires MS_REMOUNT | MS_BIND to safely change mount options
        // (e.g., ro/rw) of an existing bind mount without altering the underlying
        // filesystem's superblock. For single-instance filesystems like 'proc',
        // we skip MS_BIND to reconfigure the instance directly.
        remount_flags |= MS_BIND;
    }

    auto delay_readonly_mount = remount_t{ std::move(destination_fd), remount_flags, mount.data };
    if ((remount_flags & MS_RDONLY) == 0) {
        // if not readonly mount, just remount directly
        LINYAPS_BOX_LOG_DEBUG("remount {} directly", mount.destination);
        do_remount(delay_readonly_mount);
        maybe_refresh_root();
        return std::nullopt;
    }

    LINYAPS_BOX_LOG_DEBUG("remount delayed");
    return delay_readonly_mount;
}

class mounter
{
    void make_rootfs_private()
    {
        auto rootfs_fd = os::throw_if_error(os::fcntl_dupfd_cloexec(root.ref(), 0));
        auto rootfs_ref = rootfs_fd.ref();

        const auto &rootfs = rootfs_ref.current_path();
        LINYAPS_BOX_LOG_DEBUG("make {} private", rootfs);

        for (auto it = std::cbegin(rootfs); it != std::cend(rootfs); ++it) {
            try {
                do_propagation_mount(rootfs_fd, MS_PRIVATE);
                return;
            } catch ([[maybe_unused]] const std::system_error &e) {
                rootfs_fd = os::throw_if_error(
                  os::openat(rootfs_ref,
                             "..",
                             { os::sys::open_flag::cloexec, os::sys::access_mode::path }));
                rootfs_ref = rootfs_fd.ref();
            }
        }

        throw std::runtime_error("make rootfs private failed");
    }

public:
    explicit mounter(infra::Root rootfd, container &container)
        : container(container)
        , root(std::move(rootfd))
    {
        remounts.reserve(this->container.get().get_config().mounts.size());
    }

    void configure_rootfs()
    {
        const auto &oci_config = container.get().get_config();

        auto has_mount_ns = oci_config.linux && oci_config.linux->namespaces
          && std::any_of(oci_config.linux->namespaces->begin(),
                         oci_config.linux->namespaces->end(),
                         [](const auto &ns) {
                             return ns.type_ == oci_config::linux_t::namespace_t::type::MOUNT;
                         });
        if (!has_mount_ns) {
            LINYAPS_BOX_LOG_DEBUG("no unshared mount namespace");
            return;
        }

        // we will pivot root later
        LINYAPS_BOX_LOG_DEBUG("Configure rootfs");
        auto prop = oci_config.linux->rootfs_propagation;
        if (prop == 0) {
            prop = MS_REC | MS_PRIVATE;
        }

        // change the propagation type of rootfs mountpoint to configured type
        // otherwise bind mount will inherit the propagation type of rootfs mountpoint
        do_propagation_mount(os::throw_if_error(os::openat(
                               utils::file_descriptor_ref::cwd(),
                               "/",
                               { os::sys::open_flag::directory | os::sys::open_flag::cloexec,
                                 os::sys::access_mode::path })),
                             prop);

        // make sure the parent mountpoint of new root is private
        // pivot root will fail if it has shared propagation type
        make_rootfs_private();

        // pivot root will reset the propagation type of rootfs mountpoint
        // we need to save the propagation type to make sure the parent mountpoint of new root is
        // what we want
        container.get().set_rootfs_propagation(prop);

        LINYAPS_BOX_LOG_DEBUG("rebind container rootfs");

        oci_config::mount_t mount;
        mount.source = root.ref().current_path();
        mount.destination = ".";
        mount.vfs_flags = MS_BIND | MS_REC;
        mount.propagation_flags = MS_PRIVATE | MS_REC;
        auto ret = do_mount(container, root, mount);
        if (ret) {
            do_remount(ret.value());
        }

        // reopen rootfs after mount to refresh vfs state
        os::throw_if_error(root.reopen());

        if (oci_config.root->readonly) {
            LINYAPS_BOX_LOG_DEBUG("remount bind rootfs to readonly");
            remount_t remount;
            remount.destination_fd = os::throw_if_error(os::fcntl_dupfd_cloexec(root.ref(), 0));
            remount.flags = MS_RDONLY | MS_BIND | MS_REMOUNT;
            remounts.push_back(std::move(remount));
        }
    }

    void do_mounts()
    {
        for (const auto &mount : container.get().get_config().mounts) {
            this->mount(mount);
        }
    }

    void mount(const oci_config::mount_t &mount)
    {
        LINYAPS_BOX_LOG_DEBUG("do mount");
        if ((mount.extension_flags & oci_config::mount_t::extension::COPY_SYMLINK)
            == oci_config::mount_t::extension::COPY_SYMLINK) {
            // COPY_SYMLINK: the mount source is a host symlink; replicate it
            // as a symlink inside the rootfs instead of bind-mounting.
            if (UNLIKELY(!mount.source)) {
                throw std::invalid_argument("copy-symlink mount requires a source");
            }

            auto target = os::throw_if_error(
              os::readlinkat(utils::file_descriptor_ref::cwd(), mount.source.value()));
            if (mount.destination.has_parent_path()) {
                os::throw_if_error(root.create_directories(mount.destination.parent_path()));
            }

            auto res = root.create(mount.destination, linyaps_box::infra::symlink_spec{ target });
            if (res) {
                return;
            }

            if (res.error() != std::errc::file_exists) {
                throw std::system_error(std::move(res).error(),
                                        "create symlink for copy-symlink mount");
            }

            // EEXIST: tolerate an existing symlink with the same target.
            auto handle = os::throw_if_error(
              root.open(mount.destination,
                        { os::sys::open_flag::no_follow | os::sys::open_flag::cloexec,
                          os::sys::access_mode::path }));
            auto existing = os::throw_if_error(os::readlinkat(handle.ref(), ""));
            if (existing != target) {
                throw std::system_error(EEXIST,
                                        std::system_category(),
                                        "symlink " + mount.destination.string()
                                          + " already exists with different content");
            }
            return;
        }

        auto delay_mount = do_mount(container, root, mount);
        if (!delay_mount.has_value()) {
            return;
        }

        remounts.push_back(std::move(delay_mount).value());
    }

    void make_path_readonly()
    {
        const auto &linux = container.get().get_config().linux;
        if (!linux || !linux->readonly_paths) {
            LINYAPS_BOX_LOG_DEBUG("no readonly paths");
            return;
        }

        LINYAPS_BOX_LOG_DEBUG("make readonly paths");

        for (const auto &path : *linux->readonly_paths) {
            auto dst_res =
              root.open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::path });
            if (!dst_res) {
                const auto &err = dst_res.error();
                if (err == std::errc::no_such_file_or_directory
                    || err == std::errc::permission_denied) {
                    continue;
                }

                throw std::system_error(err, fmt::format("failed to open {} under rootfs", path));
            }

            auto dst = std::move(dst_res).value();
            auto dst_ref = dst.ref();
            auto vfs_flag = MS_BIND | MS_RDONLY | MS_REC;
            auto prop_flag = MS_PRIVATE | MS_REC;

            // readonly path is an absolute path within the container,
            // the path is already exists in the container when making it readonly
            // so we should inherit the mount flags to keep it as same as the original
            auto ret = os::throw_if_error(os::fstatfs(dst_ref));
            vfs_flag |= ret.f_flags;

            // parent mount flags may contain MS_REMOUNT, we should remove it due to the
            // readonly path is not mounted yet
            vfs_flag &= ~MS_REMOUNT;

            oci_config::mount_t mount{ };
            mount.destination = path;
            mount.source = dst_ref.proc_path();
            mount.vfs_flags = vfs_flag;
            mount.propagation_flags = prop_flag;

            LINYAPS_BOX_LOG_DEBUG("make readonly path {} with {}",
                                  path.string(),
                                  dump(mount.vfs_flags, mount.propagation_flags));
            auto delay_mount = do_mount(container, root, mount);
            if (!delay_mount) {
                throw std::runtime_error(
                  fmt::format("make readonly path {} did not produce a remount entry", path));
            }

            remounts.emplace_back(std::move(delay_mount).value());
        }
    }

    void make_path_masked()

    {
        const auto &linux = container.get().get_config().linux;
        if (!linux || !linux->masked_paths) {
            LINYAPS_BOX_LOG_DEBUG("no masked paths");
            return;
        }

        LINYAPS_BOX_LOG_DEBUG("make masked paths");

        for (const auto &path : *linux->masked_paths) {
            // we only need to open a fd to refer to the path
            // so O_PATH is sufficient.
            auto dst = root.open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::path });
            if (UNLIKELY(!dst)) {
                auto err = std::move(dst).error();
                if (err == std::errc::no_such_file_or_directory
                    || err == std::errc::permission_denied) {
                    continue;
                }

                throw std::system_error(err, fmt::format("failed to open {} under rootfs", path));
            }

            auto ret =
              os::throw_if_error(os::fstatat(dst->ref(), "", os::sys::at_flag::empty_path));
            auto mount = oci_config::mount_t{ };

            mount.destination = path;
            mount.vfs_flags = MS_RDONLY;

            if (S_ISDIR(ret.st_mode)) {
                mount.source = "tmpfs";
                mount.type = "tmpfs";
                mount.data = "size=0k";

                LINYAPS_BOX_LOG_DEBUG("mask directory {}", path.string());
                auto delay_mount = do_mount(container, root, mount);
                if (!delay_mount) {
                    throw std::runtime_error("mask directory " + path.string()
                                             + " did not produce a remount entry");
                }
                remounts.emplace_back(std::move(delay_mount).value());
                continue;
            }

            mount.source = "/dev/null";
            mount.vfs_flags |= MS_BIND;

            LINYAPS_BOX_LOG_DEBUG("mask file {}", path.string());
            auto delay_mount = do_mount(container, root, mount);
            if (!delay_mount) {
                throw std::runtime_error("mask file " + path.string()
                                         + " did not produce a remount entry");
            }
            remounts.emplace_back(std::move(delay_mount).value());
        }
    }

    void finalize()
    {
        if (!container.get().mount_dev_from_host()) {
            this->create_default_devices();
            this->ensure_dev_ptmx();
            this->configure_dev_symlinks();
        }

        LINYAPS_BOX_LOG_DEBUG("finalize {} remounts", remounts.size());
        // our mount process has to do with the order
        // the last mount should be the last remount
        std::for_each(remounts.crbegin(), remounts.crend(), do_remount);
    }

private:
    std::reference_wrapper<linyaps_box::container> container;
    infra::Root root;
    std::vector<remount_t> remounts;

    // Creates default device nodes mandated by the OCI runtime spec.
    // https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md#default-devices
    void create_default_devices()
    {
        LINYAPS_BOX_LOG_DEBUG("Create default devices");

        constexpr auto mode = std::filesystem::perms::owner_read
          | std::filesystem::perms::owner_write | std::filesystem::perms::group_read
          | std::filesystem::perms::group_write | std::filesystem::perms::others_read
          | std::filesystem::perms::others_write;

        struct device_spec
        {
            std::string_view name;
            std::filesystem::file_type type;
            dev_t dev;
        };

        static const std::array<device_spec, 6> devices{ {
          { "null", std::filesystem::file_type::character, makedev(1, 3) },
          { "zero", std::filesystem::file_type::character, makedev(1, 5) },
          { "full", std::filesystem::file_type::character, makedev(1, 7) },
          { "random", std::filesystem::file_type::character, makedev(1, 8) },
          { "urandom", std::filesystem::file_type::character, makedev(1, 9) },
          { "tty", std::filesystem::file_type::character, makedev(5, 0) },
        } };

        auto dev_fd = os::throw_if_error(
          root.open("dev", { os::sys::open_flag::cloexec, os::sys::access_mode::path }));
        auto ref = dev_fd.ref();

        for (const auto &d : devices) {
            LINYAPS_BOX_LOG_DEBUG("Creating device /dev/{} (type={}, dev={})",
                                  d.name,
                                  d.type,
                                  d.dev);
            auto ret = os::mknodat(ref, d.name, d.type, mode, d.dev);
            if (ret) {
                os::throw_if_error(
                  os::fchmodat(ref, d.name, mode, os::sys::at_flag::symlink_nofollow));
                os::throw_if_error(
                  os::fchownat(ref, d.name, 0, 0, os::sys::at_flag::symlink_nofollow));
                continue;
            }

            const auto &err = ret.error();
            LINYAPS_BOX_LOG_DEBUG("mknodat /dev/{} failed: {}", d.name, err.message());
            if (err == std::errc::file_exists) {
                continue;
            }

            // In user namespace, mknodat fails with EPERM because CAP_MKNOD is
            // not available. Fallback to bind mount the host device.
            if (err == std::errc::operation_not_permitted) {
                LINYAPS_BOX_LOG_DEBUG("fallback to bind mount /dev/{}", d.name);

                oci_config::mount_t mount;
                mount.source = std::string{ "/dev/" } + std::string{ d.name };
                mount.destination = std::string{ "/dev/" } + std::string{ d.name };
                mount.type = "bind";
                mount.vfs_flags = MS_BIND | MS_NOEXEC | MS_NOSUID;
                mount.propagation_flags = MS_PRIVATE;
                this->mount(mount);
                continue;
            }
        }
    }

    // https://github.com/opencontainers/runtime-spec/blob/main/runtime-linux.md
    void ensure_dev_ptmx()
    {
        auto ptmx_res = root.open("dev/ptmx",
                                  { os::sys::open_flag::no_follow | os::sys::open_flag::cloexec,
                                    os::sys::access_mode::path });
        if (!ptmx_res) {
            if (const auto &err = ptmx_res.error(); err != std::errc::no_such_file_or_directory) {
                throw std::system_error(err, "failed to open /dev/ptmx");
            }

            // fallback to create symlink pts/ptmx
            os::throw_if_error(
              root.create("dev/ptmx", linyaps_box::infra::symlink_spec{ "pts/ptmx" }));
            return;
        }
        auto ptmx = std::move(*ptmx_res);

        auto stat_res = os::fstat(ptmx.ref());
        if (UNLIKELY(!stat_res)) {
            throw std::system_error(stat_res.error(), "fstat /dev/ptmx");
        }
        const auto &ptmx_stat = *stat_res;

        auto type = os::to_fs_file_type(ptmx_stat.st_mode);
        switch (type) {
        case std::filesystem::file_type::regular: {
            // /dev/ptmx is a regular file: bind mount /dev/pts/ptmx over it
            oci_config::mount_t mount;
            mount.source = root.ref().current_path() / "dev/pts/ptmx";
            mount.destination = "/dev/ptmx";
            mount.type = "bind";
            mount.vfs_flags = MS_BIND | MS_NOEXEC | MS_NOSUID;
            mount.propagation_flags = MS_PRIVATE;
            this->mount(mount);
        } break;
        case std::filesystem::file_type::symlink: {
            // /dev/ptmx is a symlink: check if it points to pts/ptmx
            auto link_target = os::throw_if_error(os::readlinkat(ptmx.ref(), ""));
            if (link_target != "pts/ptmx" && link_target != "/dev/pts/ptmx") {
                // Atomically replace the symlink using a temp + rename
                os::throw_if_error(root.create("dev/.ptmx.tmp", infra::symlink_spec{ "pts/ptmx" }));
                os::throw_if_error(root.rename("dev/.ptmx.tmp", "dev/ptmx"));
            }
        } break;
        case std::filesystem::file_type::block:
        case std::filesystem::file_type::character:
        case std::filesystem::file_type::fifo:
        case std::filesystem::file_type::socket:
            [[fallthrough]];
        case std::filesystem::file_type::directory:
            throw std::runtime_error(
              fmt::format("invalid /dev/ptmx type: {}, expected a regular file or symlink", type));
        case std::filesystem::file_type::none:
        case std::filesystem::file_type::not_found:
            [[fallthrough]];
        case std::filesystem::file_type::unknown:
            throw std::runtime_error("an unexpected error occurred while checking /dev/ptmx type");
        }
    }

    // https://github.com/opencontainers/runtime-spec/blob/main/runtime-linux.md
    void configure_dev_symlinks()
    {
        LINYAPS_BOX_LOG_DEBUG("Configure dev symlinks");
        constexpr static std::array<std::pair<std::string_view, std::string_view>, 4> symlinks{
            { { "/proc/self/fd", "fd" },
              { "/proc/self/fd/0", "stdin" },
              { "/proc/self/fd/1", "stdout" },
              { "/proc/self/fd/2", "stderr" } }
        };

        auto dev_fd = os::throw_if_error(
          root.open("dev", { os::sys::open_flag::cloexec, os::sys::access_mode::path }));

        for (const auto &[src, dst] : symlinks) {
            auto res = os::symlinkat(src, dev_fd.ref(), dst);
            if (UNLIKELY(!res && res.error() != std::errc::file_exists)) {
                throw std::system_error(res.error(), "failed to create dev symlinks");
            }
        }
    }
};

void configure_mounts(container &container, const std::filesystem::path &rootfs)
{
    LINYAPS_BOX_LOG_DEBUG("=== configure_mounts START ===");
    LINYAPS_BOX_LOG_DEBUG("Configure mounts");

    const auto &oci_config = container.get_config();

    if (oci_config.mounts.empty()) {
        LINYAPS_BOX_LOG_DEBUG("Nothing to do");
        return;
    }

    auto m = std::make_unique<mounter>(os::throw_if_error(infra::Root::open(rootfs)), container);

    LINYAPS_BOX_LOG_DEBUG("Processing mount points");

    m->configure_rootfs();
    m->do_mounts();
    m->make_path_masked();
    m->make_path_readonly();
    m->finalize();

    LINYAPS_BOX_LOG_DEBUG("Mounts configured");
}

[[noreturn]] void execute_process(const oci_config &oci_config)
{
    const auto &process = *oci_config.process;

    LINYAPS_BOX_LOG_DEBUG("Execute container process:{}", [&process]() -> std::string {
        std::stringstream ss;
        ss << " " << process.args.at(0);
        std::for_each(process.args.cbegin() + 1, process.args.cend(), [&ss](const auto &arg) {
            ss << " " << arg;
        });

        return ss.str();
    }());

    std::vector<const char *> c_args;
    c_args.reserve(process.args.size() + 1);
    for (const auto &arg : process.args) {
        c_args.push_back(arg.c_str());
    }
    c_args.push_back(nullptr);

    std::vector<const char *> c_env;
    if (process.env) {
        c_env.reserve(process.env->size() + 1);
        for (const auto &env : *process.env) {
            c_env.push_back(env.c_str());
        }
    }
    c_env.push_back(nullptr);

    auto ret = ::chdir(process.cwd.c_str());
    if (ret != 0) {
        throw std::system_error(errno, std::system_category(), "chdir");
    }

    // Verify that the cwd is inside the container mount namespace.
    std::error_code ec;
    std::ignore = std::filesystem::current_path(ec);
    if (UNLIKELY(ec == std::errc::no_such_file_or_directory)) {
        throw std::runtime_error(
          "current working directory is outside the container mount namespace");
    }

    ::execvpe(c_args.at(0),
              const_cast<char *const *>(c_args.data()),
              const_cast<char *const *>(c_env.data()));

    throw std::system_error(errno, std::system_category(), "execvpe");
}

void wait_prestart_hooks_result(const oci_config &oci_config, child_message_channel &sync)
{
    if (!oci_config.hooks || !oci_config.hooks->prestart) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Request execute prestart hooks");

    sync.send_stage(stage::type::prestart_ready);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent, Wait prestart runtime result");

    sync.expect_stage(stage::type::prestart_done);

    LINYAPS_BOX_LOG_DEBUG("Prestart hooks executed");
}

void wait_create_runtime_result(const oci_config &oci_config, child_message_channel &sync)
{
    if (!oci_config.hooks || !oci_config.hooks->create_runtime) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Request execute createRuntime hooks");

    sync.send_stage(stage::type::createruntime_ready);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent, Wait create runtime result");

    sync.expect_stage(stage::type::createruntime_done);

    LINYAPS_BOX_LOG_DEBUG("Create runtime hooks executed");
}

void create_container_hooks(const container &container,
                            const container_status &status,
                            child_message_channel &sync)
{
    const auto &oci_config = container.get_config();
    if (!oci_config.hooks || !oci_config.hooks->create_container) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Execute create container hooks");

    for (const auto &hook : oci_config.hooks->create_container.value()) {
        execute_hook(hook, status);
    }

    LINYAPS_BOX_LOG_DEBUG("Create container hooks executed");

    sync.send_stage(stage::type::createcontainer_done);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent");
}

void do_pivot_root(const container &container,
                   const std::filesystem::path &rootfs,
                   bool has_mount_ns)
{
    if (!has_mount_ns) {
        LINYAPS_BOX_LOG_DEBUG("no mount namespace, fallback to chroot");
        auto ret = chdir(rootfs.c_str());
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "chdir to rootfs");
        }

        ret = chroot(".");
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "chroot");
        }

        ret = chdir("/");
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "chdir to /");
        }

        return;
    }

    auto old_root = os::throw_if_error(os::open(
      "/",
      { os::sys::open_flag::directory | os::sys::open_flag::cloexec, os::sys::access_mode::path },
      std::filesystem::perms::none));
    auto new_root = os::throw_if_error(os::open(
      rootfs,
      { os::sys::open_flag::directory | os::sys::open_flag::cloexec, os::sys::access_mode::path },
      std::filesystem::perms::none));

    auto old_root_stat = os::throw_if_error(os::fstatfs(old_root.ref()));
    LINYAPS_BOX_LOG_DEBUG("Pivot root old root: {}", dump(old_root_stat.f_flags));

    auto new_root_stat = os::throw_if_error(os::fstatfs(new_root.ref()));
    LINYAPS_BOX_LOG_DEBUG("Pivot root new root: {}", dump(new_root_stat.f_flags));

    auto ret = fchdir(new_root.get());
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "fchdir");
    }

    ret = syscall(__NR_pivot_root, ".", ".");
    if (ret < 0) {
        LINYAPS_BOX_LOG_DEBUG("pivot_root failed ({}), fallback to move_root + chroot", errno);
        // fallback: MS_MOVE + chroot
        ret = mount(rootfs.c_str(), "/", "", MS_MOVE, nullptr);
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "mount MS_MOVE");
        }

        ret = chroot(".");
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "chroot after MS_MOVE");
        }

        ret = chdir("/");
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "chdir to /");
        }

        return;
    }

    ret = fchdir(old_root.get());
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "fchdir");
    }

    // make sure that umount event couldn't propagate to host.
    // target the old root via cwd "." (set by fchdir(old_root) above); do NOT use
    // old_root.current_path(); after pivot_root(".", ".") the old_root fd's readlink
    // resolves to "/" (= new root X), which would recursively privatize X and all
    // submounts (e.g. /run/media, /sys), overriding their inherited propagation.
    syscall_mount(nullptr, ".", nullptr, MS_REC | MS_PRIVATE, nullptr);

    // umount old root
    ret = umount2(".", MNT_DETACH);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "umount2");
    }

    do {
        ret = umount2(".", MNT_DETACH);
        if (ret < 0 && errno == EINVAL) {
            break;
        }
        if (ret < 0) {
            throw std::system_error(errno, std::system_category(), "umount2");
        }
    } while (ret == 0);

    ret = chdir("/");
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "chdir");
    }

    // restore the propagation type of rootfs mountpoint
    do_propagation_mount(
      os::throw_if_error(os::open(
        "/",
        { os::sys::open_flag::directory | os::sys::open_flag::cloexec, os::sys::access_mode::path },
        std::filesystem::perms::none)),
      container.rootfs_propagation());
}

void start_container_hooks(const container &container, const container_status &status)
{
    const auto &oci_config = container.get_config();
    if (!oci_config.hooks || !oci_config.hooks->start_container) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Execute start container hooks");

    for (const auto &hook : oci_config.hooks->start_container.value()) {
        execute_hook(hook, status);
    }

    LINYAPS_BOX_LOG_DEBUG("Start container hooks executed");

    LINYAPS_BOX_LOG_DEBUG("Sync message sent");
}

void processing_extensions(const oci_config &oci_config)
{
    if (!oci_config.annotations) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Processing container extensions");

    // ext_ns_last_pid
    // This file may not exist if the kernel config CONFIG_CHECKPOINT_RESTORE is not enabled
    // and this feature originally was used for userspace checkpoint/restore
    // we use this feature for avoiding two process has the same pid.
    // e.g some application will register a tray through dbus and use the pid as the part of
    // dbus object path, if two process has the same pid, the dbus object path will conflict
    auto it = oci_config.annotations->find("cn.org.linyaps.runtime.ns_last_pid");
    while (it != oci_config.annotations->end()) {
        LINYAPS_BOX_LOG_DEBUG("Processing ns_last_pid extension: {}", it->second);

        // Validate input is a valid pid_t number
        try {
            // Use std::stoll to handle larger ranges, then check if it fits in pid_t
            const auto value = std::stoll(it->second);
            if (value < 0 || value > std::numeric_limits<pid_t>::max()) {
                throw std::runtime_error("ns_last_pid value out of range: " + it->second
                                         + " (must be between 0 and "
                                         + std::to_string(std::numeric_limits<pid_t>::max()) + ")");
            }
        } catch (const std::out_of_range &e) {
            throw std::runtime_error("parse ns_last_pid " + it->second + " failed: " + e.what());
        } catch (const std::invalid_argument &e) {
            throw std::runtime_error("parse ns_last_pid " + it->second + " failed: " + e.what());
        }

        // ignore ns_last_pid if the file does not exist
        auto ns_last_pid = std::filesystem::path{ "/proc/sys/kernel/ns_last_pid" };
        if (!std::filesystem::exists(ns_last_pid)) {
            break;
        }

        std::ofstream ofs(ns_last_pid);
        if (!ofs) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "failed to open /proc/sys/kernel/ns_last_pid");
        }

        ofs << it->second;
        if (!ofs) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "failed to write to /proc/sys/kernel/ns_last_pid");
        }

        LINYAPS_BOX_LOG_DEBUG("Successfully set ns_last_pid to {}", it->second);
        break;
    }

    LINYAPS_BOX_LOG_DEBUG("Container extensions processing completed");
}

void configure_terminal(const container &container, protocol::child_message_channel &sync)
{
    LINYAPS_BOX_LOG_DEBUG("Configure terminal");
    const auto &process = *container.get_config().process;

    auto [slave, path, master] = linyaps_box::create_pty_pair();

    slave.setup_stdio();

    auto ret = fchown(slave.fd().get(), process.user.uid, process.user.gid);
    if (ret != 0) {
        throw std::system_error(errno, std::system_category(), "fchown");
    }

    if (process.console_size) {
        slave.set_size({ process.console_size->height, process.console_size->width, 0, 0 });
    }

    auto root = os::throw_if_error(infra::Root::open("/"));

    // /dev/console must be a regular file (for bind-mount) or absent.
    // If it is a symlink, remove it
    auto console_res = root.open(
      "dev/console",
      { os::sys::open_flag::no_follow | os::sys::open_flag::cloexec, os::sys::access_mode::path });
    if (console_res) {
        auto st_res = os::fstat(console_res->ref());
        if (UNLIKELY(!st_res)) {
            throw std::system_error(std::move(st_res).error(), "fstatat /dev/console");
        }
        auto type = os::to_fs_file_type(st_res->st_mode);

        if (UNLIKELY(type == std::filesystem::file_type::symlink)) {
            LINYAPS_BOX_LOG_DEBUG("/dev/console is a symlink; removing it");
            // No TOCTOU concern: remove_file is scoped (Root guards against
            // escape), and the worst case if a racer replaces the symlink
            // between our check and the unlink is a harmless EISDIR or a
            // redundant unlink of a non-symlink — neither breaks the
            // subsequent bind mount.
            os::throw_if_error(root.remove_file("dev/console"));
        }
    } else if (console_res.error() != std::errc::no_such_file_or_directory) {
        throw std::system_error(console_res.error(), "open /dev/console");
    }

    oci_config::mount_t mount{ };
    mount.source = std::move(path);
    mount.destination = "/dev/console";
    mount.vfs_flags = MS_BIND;

    std::ignore = container_ns::do_bind_mount(root, mount);
    auto console_fd = std::move(master).take();
    auto ref = console_fd.ref();
    sync.send_console_fd(ref);
}

int clone_fn(void *data) noexcept
{
    LINYAPS_BOX_LOG_DEBUG("OCI runtime in container namespace: PID={} PIDNS={}",
                          getpid(),
                          get_pid_namespace());

    auto &args = *static_cast<clone_fn_args *>(data);

    try {
        auto &logger = log::global_logger::instance();
        logger.set_forwarder(std::make_unique<protocol::sync_socket_forwarder>(args.sync));

        if (getenv("LINYAPS_BOX_CONTAINER_PROCESS_TRACE_ME") != nullptr) {
            auto signal_USR1_handler = []([[maybe_unused]] int) {
                static constexpr char msg[] = "[DEBUG] Signal USR1 received.\n";
                std::ignore = ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
            };

            auto ret = signal(SIGUSR1, signal_USR1_handler);

            if (ret == SIG_ERR) {
                throw std::system_error(errno, std::system_category(), "signal");
            }
            assert(ret == SIG_DFL);

            LINYAPS_BOX_LOG_INFO(
              "OCI runtime in container namespace waiting for signal USR1 to continue");
            pause();

            ret = signal(SIGUSR1, SIG_DFL);
            if (ret == SIG_ERR) {
                throw std::system_error(errno, std::system_category(), "signal");
            }
            assert(ret == signal_USR1_handler);
        }

        LINYAPS_BOX_LOG_DEBUG("OCI runtime in container namespace starts");

        auto &sync = args.sync;

        utils::close_range(3U + static_cast<unsigned>(args.preserve_fds),
                           std::numeric_limits<unsigned>::max(),
                           CLOSE_RANGE_CLOEXEC);

        auto &container = *args.container;
        const auto &oci_config = container.get_config();

        auto rootfs = container.get_config().root->path;
        if (rootfs.is_relative()) {
            LINYAPS_BOX_LOG_DEBUG("rootfs is relative based on bundle path:{}",
                                  container.get_bundle());
            rootfs = std::filesystem::canonical(container.get_bundle() / rootfs);
        }

        // Prime the cap_last_cap cache before pivot_root, since
        // /proc/sys/kernel/cap_last_cap may not be available in the
        // container rootfs after the root switch.
        std::ignore = security::last_cap();

        container_ns::initialize_container(container.get_config(), sync);
        container_ns::configure_mounts(container, rootfs);
        wait_prestart_hooks_result(oci_config, sync);
        wait_create_runtime_result(oci_config, sync);

        auto status = container.status();
        create_container_hooks(container, status, sync);
        // TODO: selinux label/apparmor profile
        auto has_mount_ns = oci_config.linux && oci_config.linux->namespaces
          && std::any_of(oci_config.linux->namespaces->cbegin(),
                         oci_config.linux->namespaces->cend(),
                         [](const auto &ns) {
                             return ns.type_ == oci_config::linux_t::namespace_t::type::MOUNT;
                         });
        do_pivot_root(container, rootfs, has_mount_ns);

        // NOTE: Cache the host root fd before pivot_root,
        // so that O_PATH fd is still usable for /proc/self/fd/<fd>
        // access from both sides after the root switch if we need in the future.

        utils::setsid();
        if (container.get_config().process->terminal.value_or(false)) {
            configure_terminal(container, sync);
        }

        if (container.get_config().process->user.umask) {
            auto val = container.get_config().process->user.umask.value();
            os::throw_if_error(os::umask(val), fmt::format("failed to set umask {}", val));
        }
        // processing all extensions before drop capabilities
        processing_extensions(oci_config);

        security::privilege_context ctx{ oci_config.process->user };
        ctx.set_capabilities(oci_config.process->capabilities)
          .set_no_new_privs(oci_config.process->no_new_privileges.value_or(false));
        ctx.apply();

        start_container_hooks(container, status);

        // unblock and reset all signals before we execute the target
        sigset_t set;
        utils::sigfillset(set);
        utils::sigprocmask(SIG_UNBLOCK, set, nullptr);
        utils::reset_signals(set);

        args.sync.send_stage(protocol::stage::type::exec_ready);

        execute_process(oci_config);
        // NOTE: Child process errors are intentionally logged and then swallowed
        // here. The parent process is NOT notified via a typed error message.
        // This is by design: the child's error boundary is isolated from the
        // parent so that child failures cannot propagate as C++ exceptions into
        // the parent's control flow. The parent detects the failure via the
        // socket close (CLOEXEC on exec / process exit) and the presence of
        // forwarded error/fatal log messages during wait_for_close().
    } catch (const std::system_error &e) {
        LINYAPS_BOX_LOG_FATAL("child process error: {}", e.what());
        _exit(EXIT_FAILURE);
    } catch (const std::exception &e) {
        LINYAPS_BOX_LOG_FATAL("child process error: {}", e.what());
        _exit(EXIT_FAILURE);
    } catch (...) {
        LINYAPS_BOX_LOG_FATAL("child process error: unknown error");
        _exit(EXIT_FAILURE);
    }

    return EXIT_FAILURE;
}

} // namespace container_ns

// NOTE: All function in this namespace are running in the runtime namespace.
namespace runtime_ns {

[[nodiscard]] auto to_clone_flag(oci_config::linux_t::namespace_t::type type) noexcept
  -> unsigned int
{
    using type_t = oci_config::linux_t::namespace_t::type;
    switch (type) {
    case type_t::NONE:
        return 0;
    case type_t::IPC:
        return CLONE_NEWIPC;
    case type_t::UTS:
        return CLONE_NEWUTS;
    case type_t::MOUNT:
        return CLONE_NEWNS;
    case type_t::PID:
        return CLONE_NEWPID;
    case type_t::NET:
        return CLONE_NEWNET;
    case type_t::USER:
        return CLONE_NEWUSER;
    case type_t::CGROUP:
        return CLONE_NEWCGROUP;
    case type_t::TIME:
#ifdef CLONE_NEWTIME
        return CLONE_NEWTIME;
#else
        return 0x00000080;
#endif
    }
    __builtin_unreachable();
}

[[nodiscard]] unsigned
generate_clone_flag(const std::optional<std::vector<oci_config::linux_t::namespace_t>> &namespaces)
{

    unsigned flag = SIGCHLD;
    LINYAPS_BOX_LOG_DEBUG("Add SIGCHLD, flag=0x{:x}", flag);
    if (!namespaces) {
        return flag;
    }

    for (const auto &ns : *namespaces) {
        flag = flag | to_clone_flag(ns.type_);
        LINYAPS_BOX_LOG_DEBUG("Add {} , flag=0x{:x}", to_string_view(ns.type_), flag);
    }

    LINYAPS_BOX_LOG_DEBUG("Clone flag=0x{:x}", flag);

    return flag;
}

class child_stack
{
public:
    child_stack()
        : stack_low(mmap(nullptr,
                         LINYAPS_BOX_CLONE_CHILD_STACK_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                         -1,
                         0))
    {
        if (this->stack_low == MAP_FAILED) {
            throw std::runtime_error("mmap child stack failed");
        }
    }

    ~child_stack() noexcept
    {
        if (this->stack_low == MAP_FAILED) {
            return;
        }

        if (munmap(this->stack_low, LINYAPS_BOX_CLONE_CHILD_STACK_SIZE) == 0) {
            return;
        }

        LINYAPS_BOX_LOG_ERROR_ERRNO(errno, "munmap child stack failed");
    }

    [[nodiscard]] auto top() const noexcept -> void *
    {
        if constexpr (LINYAPS_BOX_STACK_GROWTH_DOWN) {
            return static_cast<std::byte *>(this->stack_low) + LINYAPS_BOX_CLONE_CHILD_STACK_SIZE;
        } else {
            return static_cast<std::byte *>(this->stack_low) - LINYAPS_BOX_CLONE_CHILD_STACK_SIZE;
        }
    }

private:
    void *stack_low;
};

[[nodiscard]] auto to_rlimit_resource(oci_config::process_t::rlimit_t::type_t type) noexcept -> int
{
    using type_t = oci_config::process_t::rlimit_t::type_t;
    switch (type) {
    case type_t::AS:
        return RLIMIT_AS;
    case type_t::CORE:
        return RLIMIT_CORE;
    case type_t::CPU:
        return RLIMIT_CPU;
    case type_t::DATA:
        return RLIMIT_DATA;
    case type_t::FSIZE:
        return RLIMIT_FSIZE;
    case type_t::LOCKS:
        return RLIMIT_LOCKS;
    case type_t::MEMLOCK:
        return RLIMIT_MEMLOCK;
    case type_t::MSGQUEUE:
        return RLIMIT_MSGQUEUE;
    case type_t::NICE:
        return RLIMIT_NICE;
    case type_t::NOFILE:
        return RLIMIT_NOFILE;
    case type_t::NPROC:
        return RLIMIT_NPROC;
    case type_t::RSS:
        return RLIMIT_RSS;
    case type_t::RTPRIO:
        return RLIMIT_RTPRIO;
    case type_t::RTTIME:
        return RLIMIT_RTTIME;
    case type_t::SIGPENDING:
        return RLIMIT_SIGPENDING;
    case type_t::STACK:
        return RLIMIT_STACK;
    }
    __builtin_unreachable();
}

void set_rlimits(const std::vector<oci_config::process_t::rlimit_t> &rlimits)
{
    std::for_each(rlimits.begin(),
                  rlimits.end(),
                  [](const oci_config::process_t::rlimit_t &rlimit) {
                      const struct rlimit rl{ rlimit.soft, rlimit.hard };
                      auto resource = to_rlimit_resource(rlimit.type);
                      LINYAPS_BOX_LOG_DEBUG("Set rlimit {}: Soft={}, Hard={}",
                                            to_string_view(rlimit.type),
                                            rlimit.soft,
                                            rlimit.hard);
                      if (setrlimit(resource, &rl) == -1) {
                          throw std::system_error(errno, std::system_category(), "setrlimit");
                      }
                  });
}

std::pair<int, parent_message_channel> start_container_process(container &container,
                                                               run_container_options_t &options)
{
    const auto &oci_config = container.get_config();

    auto [parent, child] = protocol::create_message_socketpair();

    // config rlimits before we enter new user namespace
    if (const auto &rlimits = oci_config.process->rlimits; rlimits) {
        set_rlimits(rlimits.value());
    }

    std::optional<std::vector<oci_config::linux_t::namespace_t>> namespaces;
    if (oci_config.linux && oci_config.linux->namespaces) {
        namespaces = oci_config.linux->namespaces;
    }

    const int clone_flag = runtime_ns::generate_clone_flag(namespaces);
    clone_fn_args args = { options.preserve_fds, &container, std::move(child) };

    LINYAPS_BOX_LOG_DEBUG("OCI runtime in runtime namespace: PID={} PIDNS={}",
                          getpid(),
                          get_pid_namespace());

    const child_stack stack;
    const int child_pid =
      clone(container_ns::clone_fn, stack.top(), clone_flag, static_cast<void *>(&args));
    if (child_pid < 0) {
        throw std::runtime_error("clone failed");
    }

    if (child_pid == 0) {
        throw std::logic_error("clone should not return in child");
    }

    return { child_pid, std::move(parent) };
}

[[nodiscard]] int execute_user_namespace_helper(const std::vector<std::string> &args)
{
    LINYAPS_BOX_LOG_DEBUG("Execute user_namespace helper:{}", [&]() -> std::string {
        std::stringstream result;
        for (const auto &arg : args) {
            result << " \"";
            for (const auto &c : arg) {
                if (c == '\\') {
                    result << "\\\\";
                } else if (c == '"') {
                    result << "\\\"";
                } else {
                    result << c;
                }
            }
            result << "\"";
        }
        return result.str();
    }());

    auto pid = fork();
    if (pid < 0) {
        throw std::system_error(errno, std::system_category(), "fork");
    }

    if (pid == 0) {
        std::vector<const char *> c_args;
        c_args.reserve(args.size());
        for (const auto &arg : args) {
            c_args.push_back(arg.c_str());
        }

        c_args.push_back(nullptr);
        execvp(c_args[0], const_cast<char *const *>(c_args.data()));
        LINYAPS_BOX_LOG_ERROR_ERRNO(errno, "execute helper {} failed", c_args[0]);
        _exit(EXIT_FAILURE);
    }

    int status = 0;
    auto ret = -1;
    while (ret == -1) {
        ret = waitpid(pid, &status, 0);

        if (ret != -1) {
            break;
        }

        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }

        throw std::system_error(errno, std::system_category(), "waitpid");
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        throw std::runtime_error("user_namespace helper exited which caused by signal "
                                 + std::to_string(WTERMSIG(status)));
    }

    throw std::runtime_error("user_namespace helper exited abnormally");
}

void set_deny_groups(container &container, const std::filesystem::path &filepath)
{
    if (container.deny_setgroups()) {
        throw std::runtime_error("denying setgroups");
    }

    auto file = os::throw_if_error(os::open(
      filepath,
      { os::sys::open_flag::cloexec | os::sys::open_flag::create | os::sys::open_flag::truncate,
        os::sys::access_mode::write_only },
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
    auto ret = ::write(file.get(), "deny", 4);
    if (ret < 0) {
        throw std::system_error{ errno, std::system_category(), "write setgroups" };
    }

    container.set_deny_setgroups();
}

void configure_gid_mapping(pid_t pid, container &container)
{
    LINYAPS_BOX_LOG_DEBUG("Configure GID mappings for PID={}", pid);
    LINYAPS_BOX_LOG_DEBUG("Configure GID mappings");

    const auto &oci_config = container.get_config();
    const auto &gid_mappings = oci_config.linux->gid_mappings;
    if (!gid_mappings) {
        LINYAPS_BOX_LOG_DEBUG("Nothing to do");
        return;
    }
    const auto &gid_mappings_v = gid_mappings.value();

    std::string content;
    const auto len = gid_mappings_v.size();
    auto self_process = std::filesystem::path{ "/proc" } / std::to_string(pid);
    const auto is_single_mapping = (gid_mappings_v.size() == 1 && gid_mappings_v[0].size == 1
                                    && gid_mappings_v[0].host_id == gid_mappings_v[0].container_id);
    if (is_single_mapping) {
        if (!container.deny_setgroups()) {
            set_deny_groups(container,
                            std::filesystem::path{ "/proc" } / std::to_string(pid) / "setgroups");
        }

        const auto &mapping = gid_mappings_v[0];
        content.append(std::to_string(mapping.host_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.container_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.size));

        auto file = os::throw_if_error(os::open(
          self_process / "gid_map",
          { os::sys::open_flag::cloexec | os::sys::open_flag::create | os::sys::open_flag::truncate,
            os::sys::access_mode::write_only },
          std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
        auto ret = ::write(file.get(), content.data(), content.size());
        if (ret > 0) {
            return;
        }

        throw std::system_error{ errno, std::system_category(), "single gid mapping failed" };
    }

    std::vector<std::string> args;
    args.emplace_back("newgidmap");
    args.push_back(std::to_string(pid));
    for (const auto &mapping : gid_mappings_v) {
        args.push_back(std::to_string(mapping.container_id));
        args.push_back(std::to_string(mapping.host_id));
        args.push_back(std::to_string(mapping.size));
    }

    auto ret = execute_user_namespace_helper(args);
    if (ret == 0) {
        return;
    }

    if (ret != ENOENT) {
        throw std::system_error(ret, std::system_category(), "newgidmap");
    }

    // maybe we have CAP_SETGID?
    content.clear();
    for (std::size_t i = 0; i < len; ++i) {
        const auto &mapping = gid_mappings_v[i];
        content.append(std::to_string(mapping.host_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.container_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.size));

        if (i != len - 1) {
            content.push_back('\n');
        }
    };

    auto file = os::throw_if_error(os::open(
      self_process / "gid_map",
      { os::sys::open_flag::cloexec | os::sys::open_flag::create | os::sys::open_flag::truncate,
        os::sys::access_mode::write_only },
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
    ret = ::write(file.get(), content.data(), content.size());
    if (ret > 0) {
        return;
    }

    throw std::system_error{ errno,
                             std::system_category(),
                             "write to " + file.ref().current_path().string() };
}

void configure_uid_mapping(pid_t pid, const container &container)
{
    LINYAPS_BOX_LOG_DEBUG("Configure UID mappings for PID={}", pid);
    LINYAPS_BOX_LOG_DEBUG("Configure UID mappings");

    const auto &oci_config = container.get_config();
    const auto &uid_mappings = oci_config.linux->uid_mappings;
    if (!uid_mappings) {
        LINYAPS_BOX_LOG_DEBUG("Nothing to do");
        return;
    }
    const auto &uid_mappings_v = uid_mappings.value();

    // If we only need to mapping a single and equivalent uids, we could write it directly.
    // This condition is the most of our usage, so try it at first instead of newuidmap

    std::string content;
    const auto len = uid_mappings_v.size();
    auto self_process = std::filesystem::path{ "/proc" } / std::to_string(pid);
    const auto is_single_mapping = (uid_mappings_v.size() == 1 && uid_mappings_v[0].size == 1
                                    && uid_mappings_v[0].host_id == uid_mappings_v[0].container_id);
    if (is_single_mapping) {
        const auto &mapping = uid_mappings_v[0];

        content.append(std::to_string(mapping.host_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.container_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.size));

        auto file = os::throw_if_error(os::open(
          self_process / "uid_map",
          { os::sys::open_flag::cloexec | os::sys::open_flag::create | os::sys::open_flag::truncate,
            os::sys::access_mode::write_only },
          std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
        auto ret = ::write(file.get(), content.data(), content.size());
        if (ret > 0) {
            return;
        }

        // NOTE: The writing process must have the same effective user ID as the process that
        // created the user namespace
        throw std::system_error{ errno, std::system_category(), "single uid mapping failed" };
    }

    // mapping multiple uid, try newuidmap at fist
    std::vector<std::string> args;
    args.emplace_back("newuidmap");
    args.push_back(std::to_string(pid));
    for (const auto &mapping : uid_mappings_v) {
        args.push_back(std::to_string(mapping.container_id));
        args.push_back(std::to_string(mapping.host_id));
        args.push_back(std::to_string(mapping.size));
    }

    auto ret = execute_user_namespace_helper(args);
    if (ret == 0) {
        return;
    }

    if (ret != ENOENT) {
        throw std::system_error(ret, std::system_category(), "newuidmap");
    }

    // try to write mapping directly, maybe we have CAP_SETUID?
    content.clear();
    for (std::size_t i = 0; i < len; ++i) {
        const auto &mapping = uid_mappings_v[i];
        content.append(std::to_string(mapping.host_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.container_id));
        content.push_back(' ');
        content.append(std::to_string(mapping.size));

        if (i != len - 1) {
            content.push_back('\n');
        }
    };

    auto file = os::throw_if_error(os::open(
      self_process / "uid_map",
      { os::sys::open_flag::cloexec | os::sys::open_flag::create | os::sys::open_flag::truncate,
        os::sys::access_mode::write_only },
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
    ret = ::write(file.get(), content.data(), content.size());
    if (ret > 0) {
        return;
    }

    throw std::system_error{ errno,
                             std::system_category(),
                             "write to " + file.ref().current_path().string() };
}

void configure_container_cgroup([[maybe_unused]] const container &container)
{
    LINYAPS_BOX_LOG_DEBUG("Configure container cgroup");
    // TODO: impl
    // enter cgroup -> wait container ready -> enter finalize ->
    // do some other settings -> configuration done
}

void configure_container_namespaces(container &container, parent_message_channel &sync)
{
    LINYAPS_BOX_LOG_DEBUG(
      "Waiting OCI runtime in container namespace to request configure namespace");

    sync.wait_for_stage(stage::type::namespace_ready);

    LINYAPS_BOX_LOG_DEBUG("Start configure namespaces");

    if (auto pid = container.status().pid; pid > 0) {
        LINYAPS_BOX_LOG_DEBUG("Container PID={}", pid);
    }

    const auto &linux = container.get_config().linux;
    if (linux) {
        const auto &namespaces = linux->namespaces;
        if (namespaces) {
            for (const auto &ns : *namespaces) {
                if (ns.path) {
                    validate_namespace_path(ns);
                }
            }

            if (std::find_if(namespaces->cbegin(),
                             namespaces->cend(),
                             [](const oci_config::linux_t::namespace_t &ns) -> bool {
                                 return ns.type_ == oci_config::linux_t::namespace_t::type::USER;
                             })
                != namespaces->end()) {
                auto pid = container.status().pid;

                // TODO: if not mapping a range of uid/gid, we could set uid/gid in the
                // container process
                if (const auto &uid_mappings = linux->uid_mappings; uid_mappings) {
                    configure_uid_mapping(pid, container);
                }

                if (const auto &gid_mappings = linux->gid_mappings; gid_mappings) {
                    configure_gid_mapping(pid, container);
                }
            }
        }
    }

    configure_container_cgroup(container);

    LINYAPS_BOX_LOG_DEBUG("Container namespaces configured");

    sync.send_stage(stage::type::namespace_done);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent");
}

void prestart_hooks(const container &container, parent_message_channel &sync)
{
    if (!container.get_config().hooks || !container.get_config().hooks->prestart) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Waiting request to execute prestart hooks");

    sync.wait_for_stage(stage::type::prestart_ready);

    LINYAPS_BOX_LOG_DEBUG("Execute prestart hooks");

    auto state = container.status();
    for (const auto &hook : container.get_config().hooks->prestart.value()) {
        execute_hook(hook, state);
    }

    LINYAPS_BOX_LOG_DEBUG("Prestart hooks executed");

    sync.send_stage(stage::type::prestart_done);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent");
}

void create_runtime_hooks(const container &container, parent_message_channel &sync)
{
    if (!container.get_config().hooks || !container.get_config().hooks->create_runtime) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Waiting request to execute create runtime hooks");

    sync.wait_for_stage(stage::type::createruntime_ready);

    LINYAPS_BOX_LOG_DEBUG("Execute create runtime hooks");

    auto state = container.status();
    for (const auto &hook : container.get_config().hooks->create_runtime.value()) {
        execute_hook(hook, state);
    }

    LINYAPS_BOX_LOG_DEBUG("Create runtime hooks executed");

    sync.send_stage(stage::type::createruntime_done);

    LINYAPS_BOX_LOG_DEBUG("Sync message sent");
}

void wait_create_container_result(const container &container, parent_message_channel &sync)
{
    if (!container.get_config().hooks || !container.get_config().hooks->create_container) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG(
      "Waiting OCI runtime in container namespace send create container hooks result");

    sync.wait_for_stage(stage::type::createcontainer_done);

    LINYAPS_BOX_LOG_DEBUG("Create container hooks executed");
}

void wait_container_started(parent_message_channel &sync)
{
    LINYAPS_BOX_LOG_DEBUG("Waiting for container process to start");
    sync.wait_for_stage(stage::type::exec_ready);
    sync.wait_for_close();
    LINYAPS_BOX_LOG_DEBUG("Container process started successfully");
}

void poststart_hooks(const container &container)
{
    if (!container.get_config().hooks || !container.get_config().hooks->poststart) {
        return;
    }

    auto state = container.status();
    for (const auto &hook : container.get_config().hooks->poststart.value()) {
        execute_hook(hook, state);
    }
}

void poststop_hooks(const container &container) noexcept
{
    if (!container.get_config().hooks || !container.get_config().hooks->poststop) {
        return;
    }

    auto state = container.status();
    for (const auto &hook : container.get_config().hooks->poststop.value()) {
        try {
            execute_hook(hook, state);
        } catch (const std::exception &e) {
            LINYAPS_BOX_LOG_ERROR("execute poststop hook {} failed: {}", hook.path, e.what());
        }
    }
}

} // namespace runtime_ns

} // namespace

container::container(status_directory status_dir, const create_container_options_t &options)
    : container_ref(std::move(status_dir), options.ID)
    , bundle(std::filesystem::canonical(options.bundle))
{
    auto config_path = options.config;
    if (config_path.is_relative()) {
        config_path = bundle / config_path;
    }

    LINYAPS_BOX_LOG_DEBUG("load oci_config from {}", config_path);
    this->config = oci_config::parse(config_path);
    auto &mount = this->config.mounts;
    std::for_each(mount.begin(), mount.end(), [this](oci_config::mount_t &mount) {
        if (mount.destination.is_relative()) {
            throw std::runtime_error("destination of mount point is relative");
        }

        if ((mount.vfs_flags & MS_BIND) == 0) {
            return;
        }

        if (!mount.source) {
            throw std::runtime_error("bind mount must has a source");
        }

        auto file = std::filesystem::path(mount.source.value());
        if (file.is_absolute()) {
            return;
        }

        auto abs_src = std::filesystem::canonical(bundle / file);
        mount.source = abs_src.string();
    });

    host_uid_ = ::geteuid();
    host_gid_ = ::getegid();

    this->status_dir().save_config(config_path);

    switch (options.manager) {
    case cgroup_manager_t::disabled: {
        this->manager = std::make_unique<disabled_cgroup_manager>();
    } break;
    case cgroup_manager_t::systemd:
    case cgroup_manager_t::cgroupfs:
        throw std::runtime_error("unsupported cgroup manager");
    }
}

const oci_config &container::get_config() const
{
    return this->config;
}

const std::filesystem::path &container::get_bundle() const
{
    return this->bundle;
}

// maybe we need a internal run function?
int container::run(run_container_options_t options)
{
    int container_process_exit_code{ EXIT_FAILURE };

    os::throw_if_error(os::set_child_subreaper(true));

    // Declared outside try so catch blocks can access it for cleanup
    std::optional<container_monitor> monitor;

    try {
        // TODO: there are some thing that should be done before starting the container process
        // e.g. do something before creating cgroup by selecting manager, selinux label, seccomp
        // setup, etc.

        // block all signals so that we can't be interrupted
        sigset_t set;
        utils::sigfillset(set);
        sigdelset(&set, SIGUSR1); // for debug
        utils::sigprocmask(SIG_BLOCK, set, nullptr);

        umask(0);

        // TODO: cgroup preenter
        auto [child_pid, sync] = runtime_ns::start_container_process(*this, options);

        monitor.emplace(child_pid);

        container_status status;
        status.oci_version = oci_config::version;
        status.id = this->get_id();
        status.pid = child_pid;
        status.bundle = this->bundle;
        status.created = std::chrono::system_clock::now();

        // TODO: use clone3 to eliminate racing window
        auto child_handle = infra::process_handle::open(child_pid);
        if (UNLIKELY(!child_handle)) {
            throw std::runtime_error(fmt::format("failed to open container process {}: {}",
                                                 child_pid,
                                                 child_handle.error().message()));
        }

        auto stat = child_handle->status();
        if (UNLIKELY(!stat)) {
            throw std::runtime_error(fmt::format("failed to get container process start time: {}",
                                                 stat.error().message()));
        }
        status.process_start_time = stat->start_time;

        std::string owner;
#ifndef LINYAPS_BOX_STATIC_LINK
        auto *pw = getpwuid(host_uid_);
        if (pw != nullptr) {
            owner = pw->pw_name;
        }
#endif
        status.owner = owner;

        if (this->config.annotations) {
            status.annotations = *this->config.annotations;
        }

        this->status_dir().write(status);

        runtime_ns::configure_container_namespaces(*this, sync);
        runtime_ns::prestart_hooks(*this, sync);
        runtime_ns::create_runtime_hooks(*this, sync);
        runtime_ns::wait_create_container_result(*this, sync);

        std::optional<terminal_master> master;
        if (config.process->terminal.value_or(false)) {
            auto console_inc = sync.drain_logs();
            std::visit(utils::Overload{
                         [&](const protocol::msg::console_fd &) {
                             auto fds = console_inc.take_fds();
                             auto master_fd = std::move(fds.front());

                             if (options.console_socket) {
                                 os::throw_if_error(
                                   options.console_socket->send_fd(master_fd.ref()));
                             } else {
                                 master = terminal_master{ std::move(master_fd) };
                             }
                         },
                         [&](const auto &) {
                             throw std::runtime_error("expected console_fd during start");
                         },
                       },
                       console_inc.body);
        }

        runtime_ns::wait_container_started(sync);

        runtime_ns::poststart_hooks(*this);

        // TODO: support detach from the parent's process
        // Now we wait for the container process to exit
        monitor->enable_signal_forwarding();

        auto in = utils::file_descriptor{ STDIN_FILENO, false };
        auto out = utils::file_descriptor{ STDOUT_FILENO, false };

        bool changed{ false };
        auto in_flags = in.flags();
        auto out_flags = out.flags();

        auto restore_if_changed = utils::make_defer([&]() noexcept {
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

        [&master, &monitor, &in, &out, &changed]() -> void {
            if (!master) {
                return;
            }

            LINYAPS_BOX_LOG_DEBUG("Container requires a terminal");

            in.set_nonblock(true);
            out.set_nonblock(true);
            changed = true;

            monitor->enable_io_forwarding(std::move(*master), in, out);
        }();

        container_process_exit_code = monitor->wait_container_exit();

        runtime_ns::poststop_hooks(*this);
    } catch (const std::exception &e) {
        if (monitor) {
            monitor->kill_child();
        }

        LINYAPS_BOX_LOG_ERROR("failed to run a container, caused by: {}", e.what());
    }

    this->status_dir().remove();

    // TODO: cleanup cgroup

    return container_process_exit_code;
}

void container::cgroup_preenter(const cgroup_options &options, utils::file_descriptor &dirfd)
{
    auto type = utils::get_cgroup_type();
    if (type != utils::cgroup_t::unified) {
        return;
    }

    this->manager->precreate_cgroup(options, dirfd);
}
