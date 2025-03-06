// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container.h"

#include "linyaps_box/impl/disabled_cgroup_manager.h"
#include "linyaps_box/utils/cgroups.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/fstat.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/mkdir.h"
#include "linyaps_box/utils/mknod.h"
#include "linyaps_box/utils/open_file.h"
#include "linyaps_box/utils/socketpair.h"
#include "linyaps_box/utils/touch.h"

#include <linux/magic.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <sys/sysmacros.h>

#include <algorithm>
#include <cassert>
#include <csignal>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

#include <dirent.h>
#include <grp.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

enum class sync_message : std::uint8_t {
    REQUEST_CONFIGURE_NAMESPACE,
    NAMESPACE_CONFIGURED,
    REQUEST_CREATERUNTIME_HOOKS,
    CREATE_RUNTIME_HOOKS_EXECUTED,
    CREATE_CONTAINER_HOOKS_EXECUTED,
    START_CONTAINER_HOOKS_EXECUTED,
};

std::stringstream &&operator<<(std::stringstream &&os, sync_message message)
{
    switch (message) {
    case sync_message::REQUEST_CONFIGURE_NAMESPACE: {
        os << "REQUEST_CONFIGURE_NAMESPACE";
    } break;
    case sync_message::NAMESPACE_CONFIGURED: {
        os << "NAMESPACE_CONFIGURED";
    } break;
    case sync_message::REQUEST_CREATERUNTIME_HOOKS: {
        os << "REQUEST_PRESTART_AND_CREATERUNTIME_HOOKS";
    } break;
    case sync_message::CREATE_RUNTIME_HOOKS_EXECUTED: {
        os << "CREATE_RUNTIME_HOOKS_EXECUTED";
    } break;
    case sync_message::CREATE_CONTAINER_HOOKS_EXECUTED: {
        os << "CREATE_CONTAINER_HOOKS_EXECUTED";
    } break;
    case sync_message::START_CONTAINER_HOOKS_EXECUTED: {
        os << "START_CONTAINER_HOOKS_EXECUTED";
    } break;
    default: {
        assert(false);
        os << "UNKNOWN " << (uint8_t)message;
    } break;
    }
    return std::move(os);
}

class unexpected_sync_message : public std::logic_error
{
public:
    unexpected_sync_message(sync_message excepted, sync_message actual)
        : std::logic_error((std::stringstream() << "unexpected sync message: expected " << excepted
                                                << " got " << actual)
                                   .str())
    {
    }
};

void execute_hook(const linyaps_box::config::hooks_t::hook_t &hook)
{
    auto pid = fork();
    if (pid < 0) {
        throw std::system_error(errno, std::generic_category(), "fork");
    }

    if (pid == 0) {
        [&]() noexcept {
            std::vector<const char *> c_args{ hook.path.c_str() };
            for (const auto &arg : hook.args) {
                c_args.push_back(arg.c_str());
            }
            c_args.push_back(nullptr);

            std::vector<std::string> envs;
            envs.reserve(hook.env.size());
            for (const auto &env : hook.env) {
                envs.push_back(env.first + "=" + env.second);
            }

            std::vector<const char *> c_env;
            c_env.reserve(envs.size());
            for (const auto &env : envs) {
                c_env.push_back(env.c_str());
            }

            execvpe(c_args[0],
                    const_cast<char *const *>(c_args.data()),
                    const_cast<char *const *>(c_env.data()));

            std::cerr << "execvp: " << strerror(errno) << " errno=" << errno << std::endl;
            exit(1);
        }();
    }

    int status = 0;

    pid_t ret = -1;
    while (ret == -1) {
        ret = waitpid(pid, &status, 0);
        if (ret != -1) {
            break;
        }
        if (errno == EINTR && errno == EAGAIN) {
            continue;
        }
        throw std::system_error(errno,
                                std::generic_category(),
                                (std::stringstream() << "waitpid " << pid).str());
    }

    if (WIFEXITED(status)) {
        return;
    }

    throw std::runtime_error((std::stringstream() << "hook terminated by signal" << WTERMSIG(status)
                                                  << " with " << WEXITSTATUS(status))
                                     .str());
}

struct clone_fn_args
{
    const linyaps_box::container *container;
    const linyaps_box::config::process_t *process;
    linyaps_box::utils::file_descriptor socket;
};

// NOTE: All function in this namespace are running in the container namespace.
namespace container_ns {

void configure_container_namespaces(linyaps_box::utils::file_descriptor &socket)
{
    LINYAPS_BOX_DEBUG() << "Request OCI runtime in runtime namespace to configure namespace";

    auto byte = std::byte(sync_message::REQUEST_CONFIGURE_NAMESPACE);
    socket << byte;
    socket >> byte;
    auto message = sync_message(byte);
    if (message != sync_message::NAMESPACE_CONFIGURED) {
        throw unexpected_sync_message(sync_message::NAMESPACE_CONFIGURED, message);
    }

    LINYAPS_BOX_DEBUG() << "Container namespaces configured from runtime namespace";
}

void system_call_mount(const char *_special_file,
                       const char *_dir,
                       const char *_fstype,
                       unsigned long int _rwflag,
                       const void *_data)
{
    LINYAPS_BOX_DEBUG() << "mount\n"
                        << "\t_special_file = " << [=]() -> std::string {
        if (_special_file == nullptr) {
            return "nullptr";
        }
        if (strncmp("/proc/self/fd/", _special_file, sizeof("/proc/self/fd/") - 1) == 0) {
            return linyaps_box::utils::inspect_fd(
                    atoi(_special_file + sizeof("/proc/self/fd/") - 1));
        }
        return _special_file;
    }() << "\n\t_dir = "
        << [=]() -> std::string {
        if (_dir == nullptr) {
            return "nullptr";
        }
        if (strncmp("/proc/self/fd/", _dir, sizeof("/proc/self/fd/") - 1) == 0) {
            return linyaps_box::utils::inspect_fd(atoi(_dir + sizeof("/proc/self/fd/") - 1));
        }
        return _dir;
    }() << "\n\t_fstype = "
        <<
            [=]() {
                if (_fstype == nullptr) {
                    return "nullptr";
                }
                return _fstype;
            }()
        << "\n\t__rwflag = " <<
            [=]() {
                std::stringstream ss;
                ss << "[ ";
                if (_rwflag & MS_RDONLY) {
                    ss << "MS_RDONLY ";
                }
                if (_rwflag & MS_NOSUID) {
                    ss << "MS_NOSUID ";
                }
                if (_rwflag & MS_NODEV) {
                    ss << "MS_NODEV ";
                }
                if (_rwflag & MS_NOEXEC) {
                    ss << "MS_NOEXEC ";
                }
                if (_rwflag & MS_SYNCHRONOUS) {
                    ss << "MS_SYNCHRONOUS ";
                }
                if (_rwflag & MS_REMOUNT) {
                    ss << "MS_REMOUNT ";
                }
                if (_rwflag & MS_MANDLOCK) {
                    ss << "MS_MANDLOCK ";
                }
                if (_rwflag & MS_DIRSYNC) {
                    ss << "MS_DIRSYNC ";
                }
                if (_rwflag & LINGYAPS_MS_NOSYMFOLLOW) {
                    ss << "MS_NOSYMFOLLOW ";
                }
                if (_rwflag & MS_NOATIME) {
                    ss << "MS_NOATIME ";
                }
                if (_rwflag & MS_NODIRATIME) {
                    ss << "MS_NODIRATIME ";
                }
                if (_rwflag & MS_BIND) {
                    ss << "MS_BIND ";
                }
                if (_rwflag & MS_MOVE) {
                    ss << "MS_MOVE ";
                }
                if (_rwflag & MS_REC) {
                    ss << "MS_REC ";
                }
                if (_rwflag & MS_SILENT) {
                    ss << "MS_SILENT ";
                }
                if (_rwflag & MS_POSIXACL) {
                    ss << "MS_POSIXACL ";
                }
                if (_rwflag & MS_UNBINDABLE) {
                    ss << "MS_UNBINDABLE ";
                }
                if (_rwflag & MS_PRIVATE) {
                    ss << "MS_PRIVATE ";
                }
                if (_rwflag & MS_SLAVE) {
                    ss << "MS_SLAVE ";
                }
                if (_rwflag & MS_SHARED) {
                    ss << "MS_SHARED ";
                }
                if (_rwflag & MS_RELATIME) {
                    ss << "MS_RELATIME ";
                }
                if (_rwflag & MS_KERNMOUNT) {
                    ss << "MS_KERNMOUNT ";
                }
                if (_rwflag & MS_I_VERSION) {
                    ss << "MS_I_VERSION ";
                }
                if (_rwflag & MS_STRICTATIME) {
                    ss << "MS_STRICTATIME ";
                }
                if (_rwflag & MS_LAZYTIME) {
                    ss << "MS_LAZYTIME ";
                }
                if (_rwflag & MS_ACTIVE) {
                    ss << "MS_ACTIVE ";
                }
                if (_rwflag & MS_NOUSER) {
                    ss << "MS_NOUSER ";
                }
                ss << "]";
                return ss.str();
            }()
        << "\n\t__data = " << [=]() {
               if (_data == nullptr) {
                   return "nullptr";
               }
               return reinterpret_cast<const char *>(_data);
           }();

    int ret = ::mount(_special_file, _dir, _fstype, _rwflag, _data);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "mount");
    }
}

struct delay_readonly_mount_t
{
    linyaps_box::utils::file_descriptor destination_fd;
    unsigned long flags;
};

void do_delay_readonly_mount(const delay_readonly_mount_t &mount)
{
    assert(mount.destination_fd.get() != -1);
    assert(mount.flags & (MS_BIND | MS_REMOUNT | MS_RDONLY));

    system_call_mount(nullptr,
                      mount.destination_fd.proc_path().c_str(),
                      nullptr,
                      mount.flags,
                      nullptr);
}

[[nodiscard]] linyaps_box::utils::file_descriptor create_destination_file(
        const linyaps_box::utils::file_descriptor &root, const std::filesystem::path &destination)
{
    LINYAPS_BOX_DEBUG() << "Creating file " << destination.string() << " under "
                        << linyaps_box::utils::inspect_path(root.get());
    const auto &parent = linyaps_box::utils::mkdir(root, destination.parent_path());
    return linyaps_box::utils::touch(parent, destination.filename());
}

[[nodiscard]] linyaps_box::utils::file_descriptor create_destination_directory(
        const linyaps_box::utils::file_descriptor &root, const std::filesystem::path &destination)
{
    LINYAPS_BOX_DEBUG() << "Creating directory " << destination.string() << " under "
                        << linyaps_box::utils::inspect_path(root.get());
    return linyaps_box::utils::mkdir(root, destination);
}

[[nodiscard]] linyaps_box::utils::file_descriptor
create_destination_symlink(const linyaps_box::utils::file_descriptor &root,
                           const std::filesystem::path &source,
                           std::filesystem::path destination)
{
    auto ret = std::filesystem::read_symlink(source);
    auto parent = linyaps_box::utils::mkdir(root, destination.parent_path());

    LINYAPS_BOX_DEBUG() << "Creating symlink " << destination.string() << " under "
                        << linyaps_box::utils::inspect_path(root.get()) << " point to " << ret;

    if (destination.is_absolute()) {
        destination = destination.lexically_relative("/");
    }

    if (symlinkat(ret.c_str(), root.get(), destination.c_str()) != -1) {
        return linyaps_box::utils::open_at(root, destination, O_PATH | O_NOFOLLOW | O_CLOEXEC);
    }

    if (errno == EEXIST) {
        std::array<char, PATH_MAX + 1> buf{};
        auto to = ::readlinkat(root.get(), destination.c_str(), buf.data(), buf.size());
        if (to == -1) {
            throw std::system_error(errno, std::system_category(), "readlinkat");
        }

        if (std::string_view{ buf.data(), static_cast<size_t>(to) } == ret) {
            return linyaps_box::utils::open_at(root, destination, O_PATH | O_NOFOLLOW | O_CLOEXEC);
        }

        throw std::system_error(errno,
                                std::system_category(),
                                "symlink " + destination.string()
                                        + " already exists with a different content");
    }

    throw std::system_error(errno, std::system_category(), "symlinkat");
}

[[nodiscard]] linyaps_box::utils::file_descriptor
ensure_mount_destination(bool isDir,
                         const linyaps_box::utils::file_descriptor &root,
                         const linyaps_box::config::mount_t &mount)
try {
    assert(mount.destination.has_value());
    auto open_flag = O_PATH | O_CLOEXEC;
    if (mount.flags | LINGYAPS_MS_NOSYMFOLLOW) {
        open_flag |= O_NOFOLLOW;
    }

    return linyaps_box::utils::open_at(root, mount.destination.value(), open_flag);
} catch (const std::system_error &e) {
    if (e.code().value() != ENOENT) {
        throw e;
    }

    auto path = mount.destination.value();
    LINYAPS_BOX_DEBUG() << "Destination " << path << " not exists: " << e.what();

    // NOTE: Automatically create destination is not a part of the OCI runtime
    // spec, as it requires implementation to follow the behavior of mount(8).
    // But both crun and runc does this.

    if (isDir) {
        return create_destination_directory(root, path);
    }

    return create_destination_file(root, path);
}

void do_propagation_mount(const linyaps_box::utils::file_descriptor &destination,
                          const unsigned long &flags)
{
    assert(destination.get() != -1);

    if (flags == 0) {
        return;
    }

    system_call_mount(nullptr, destination.proc_path().c_str(), nullptr, flags, nullptr);
}

void do_bind_mount(const linyaps_box::utils::file_descriptor &root,
                   const linyaps_box::config::mount_t &mount)
{
    assert(mount.flags & MS_BIND);
    assert(mount.source.has_value());
    assert(mount.destination.has_value());

    LINYAPS_BOX_DEBUG() << "Bind mount host " << mount.source.value() << " to container "
                        << mount.destination.value().string();

    if ((mount.extra_flags & linyaps_box::config::mount_t::COPY_SYMLINK) != 0) {
        auto ret =
                create_destination_symlink(root, mount.source.value(), mount.destination.value());
        return;
    }

    auto open_flag = O_PATH;
    if ((mount.flags & LINGYAPS_MS_NOSYMFOLLOW) != 0) {
        open_flag |= O_NOFOLLOW;
    }
    auto source_fd = linyaps_box::utils::open(mount.source.value(), open_flag);
    auto source_stat = linyaps_box::utils::lstat(source_fd);

    linyaps_box::utils::file_descriptor destination_fd;
    auto sourceIsDir = S_ISDIR(source_stat.st_mode);
    destination_fd = ensure_mount_destination(sourceIsDir, root, mount);

    auto bind_flags = mount.flags & (MS_BIND | MS_REC);
    system_call_mount(source_fd.proc_path().c_str(),
                      destination_fd.proc_path().c_str(),
                      nullptr,
                      bind_flags,
                      nullptr);

    destination_fd = ensure_mount_destination(sourceIsDir, root, mount);
    bind_flags = mount.flags | MS_REMOUNT;
    system_call_mount(nullptr, destination_fd.proc_path().c_str(), nullptr, bind_flags, nullptr);

    if (mount.propagation_flags == 0) {
        return;
    }

    do_propagation_mount(destination_fd, mount.propagation_flags);
}

[[nodiscard]] std::optional<delay_readonly_mount_t>
do_mount(const linyaps_box::utils::file_descriptor &root, const linyaps_box::config::mount_t &mount)
{
    if ((mount.flags & MS_BIND) != 0) {
        do_bind_mount(root, mount);
        return std::nullopt;
    }

    LINYAPS_BOX_DEBUG() << "Mount " << [&]() -> std::string {
        std::stringstream result;
        if (!mount.type.empty()) {
            result << mount.type << ":";
        }
        result << mount.source.value_or("none");
        return result.str();
    }() << " to "
        << mount.destination.value().string();

    linyaps_box::utils::file_descriptor destination_fd =
            ensure_mount_destination(true, root, mount);

    auto mount_flags = mount.flags;

    std::optional<delay_readonly_mount_t> delay_readonly_mount;

    if (mount.type == "tmpfs" && ((mount.flags & MS_RDONLY) != 0)) {
        delay_readonly_mount =
                delay_readonly_mount_t{ std::move(destination_fd),
                                        mount_flags | MS_RDONLY | MS_REMOUNT | MS_BIND };
        mount_flags &= ~MS_RDONLY;
    }

    system_call_mount(mount.source ? mount.source.value().c_str() : nullptr,
                      destination_fd.proc_path().c_str(),
                      mount.type.c_str(),
                      mount_flags,
                      mount.data.c_str());

    do_propagation_mount(destination_fd, mount.propagation_flags);

    return delay_readonly_mount;
}

class mounter
{
public:
    explicit mounter(linyaps_box::utils::file_descriptor root)
        : root(std::move(root))
    {
    }

    void mount(const linyaps_box::config::mount_t &mount)
    {
        auto delay_mount = do_mount(root, mount);
        if (!delay_mount.has_value()) {
            return;
        }

        remounts.push_back(std::move(delay_mount).value());
    }

    void finalize()
    {
        this->configure_default_filesystems();
        this->configure_default_devices();
        for (const auto &remount : remounts) {
            do_delay_readonly_mount(remount);
        }
    }

private:
    linyaps_box::utils::file_descriptor root;
    std::vector<delay_readonly_mount_t> remounts;

    // https://github.com/opencontainers/runtime-spec/blob/09fcb39bb7185b46dfb206bc8f3fea914c674779/config-linux.md#default-filesystems
    void configure_default_filesystems()
    {
        do {
            auto proc = linyaps_box::utils::open_at(root, "proc");
            struct statfs buf{};
            int ret = ::statfs(proc.proc_path().c_str(), &buf);
            if (ret != 0) {
                throw std::system_error(errno, std::generic_category(), "statfs");
            }

            if (buf.f_type == PROC_SUPER_MAGIC) {
                break;
            }

            linyaps_box::config::mount_t mount;
            mount.source = "proc";
            mount.type = "proc";
            mount.destination = "/proc";
            this->mount(mount);
        } while (false);

        do {
            auto sys = linyaps_box::utils::open_at(root, "sys");
            struct statfs buf{};
            if (::statfs(sys.proc_path().c_str(), &buf) != 0) {
                throw std::system_error(errno, std::generic_category(), "statfs");
            }

            if (buf.f_type == SYSFS_MAGIC) {
                break;
            }

            linyaps_box::config::mount_t mount;
            mount.source = "sysfs";
            mount.type = "sysfs";
            mount.destination = "/sys";
            mount.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;
            try {
                this->mount(mount);
            } catch (const std::system_error &e) {
                if (e.code().value() != EPERM) {
                    throw;
                }

                // NOTE: fallback to bind mount
                mount.source = "/sys";
                mount.type = "bind";
                mount.destination = "/sys";
                mount.flags = MS_BIND | MS_REC | MS_NOSUID | MS_NOEXEC | MS_NODEV;
                this->mount(mount);
            }
        } while (false);

        do {
            auto dev = linyaps_box::utils::open_at(root, "dev");
            struct statfs buf{};
            int ret = ::statfs(dev.proc_path().c_str(), &buf);
            if (ret != 0) {
                throw std::system_error(errno, std::generic_category(), "statfs");
            }

            if (buf.f_type == TMPFS_MAGIC) {
                break;
            }

            if (!std::filesystem::is_empty(dev.proc_path())) {
                break;
            }

            linyaps_box::config::mount_t mount;
            mount.source = "tmpfs";
            mount.destination = "/dev";
            mount.type = "tmpfs";
            mount.flags = MS_NOSUID | MS_STRICTATIME | MS_NODEV;
            mount.data = "mode=755,size=65536k";
            this->mount(mount);
        } while (false);

        do {
            try {
                auto pts = linyaps_box::utils::open_at(root, "dev/pts");
                break;
            } catch (const std::system_error &e) {
                if (e.code().value() != ENOENT) {
                    throw;
                }
            }

            linyaps_box::config::mount_t mount;
            mount.source = "devpts";
            mount.destination = "/dev/pts";
            mount.type = "devpts";
            mount.flags = MS_NOSUID | MS_NOEXEC;
            mount.data = "newinstance,ptmxmode=0666,mode=0620";
            this->mount(mount);
        } while (false);

        do {
            try {
                auto shm = linyaps_box::utils::open_at(root, "dev/shm");
                break;
            } catch (const std::system_error &e) {
                if (e.code().value() != ENOENT) {
                    throw;
                }
            }

            linyaps_box::config::mount_t mount;
            mount.source = "shm";
            mount.destination = "/dev/shm";
            mount.type = "tmpfs";
            mount.flags = MS_NOSUID | MS_NOEXEC | MS_NODEV;
            mount.data = "mode=1777,size=65536k";
            this->mount(mount);
        } while (false);
    }

    void configure_deivce(const std::filesystem::path &destination, mode_t mode, dev_t dev)
    {
        assert(destination.is_absolute());

        std::optional<linyaps_box::utils::file_descriptor> destination_fd;
        try {
            destination_fd = linyaps_box::utils::open_at(root, destination.relative_path(), O_PATH);
        } catch (const std::system_error &e) {
            if (e.code().value() != ENOENT) {
                throw;
            }
        }

        if (!destination_fd.has_value()) {
            try {
                linyaps_box::utils::mknod(root, destination.relative_path(), mode, dev);
            } catch (const std::system_error &e) {
                if (e.code().value() != EPERM) {
                    throw;
                }
            }
        }

        auto stat = linyaps_box::utils::lstat(*destination_fd);
        if (S_ISCHR(stat.st_mode) && major(stat.st_dev) == 1 && minor(stat.st_dev) == 3) {
            return;
        }

        // NOTE: fallback to bind mount host device into container

        linyaps_box::config::mount_t mount;
        mount.source = destination;
        mount.destination = destination;
        mount.type = "bind";
        mount.flags = MS_BIND | MS_REC | MS_NOSUID | MS_NOEXEC | MS_NODEV;
        this->mount(mount);
    }

    void configure_default_devices()
    {
        this->configure_deivce("/dev/null", 0666, makedev(1, 3));
        this->configure_deivce("/dev/zero", 0666, makedev(1, 5));
        this->configure_deivce("/dev/full", 0666, makedev(1, 7));
        this->configure_deivce("/dev/random", 0666, makedev(1, 8));
        this->configure_deivce("/dev/urandom", 0666, makedev(1, 9));
        this->configure_deivce("/dev/tty", 0666, makedev(5, 0));

        // TODO Handle `/dev/console`;

        // TODO Handle `/dev/ptmx`;
    }
};

void configure_mounts(const linyaps_box::container &container)
{
    LINYAPS_BOX_DEBUG() << "Configure mounts";

    if (container.get_config().mounts.empty()) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
        return;
    }

    std::unique_ptr<mounter> m;

    {
        auto bundle = linyaps_box::utils::open(container.get_bundle(), O_PATH);
        m = std::make_unique<mounter>(
                linyaps_box::utils::open_at(bundle, container.get_config().root.path, O_PATH));
    }

    // TODO: if root is read only, add it to remount list

    LINYAPS_BOX_DEBUG() << "Processing mount points";

    for (const auto &mount : container.get_config().mounts) {
        m->mount(mount);
    }

    m->finalize();

    LINYAPS_BOX_DEBUG() << "Mounts configured";
}

[[noreturn]] void execute_process(const linyaps_box::config::process_t &process)
{
    std::vector<const char *> c_args;
    c_args.reserve(process.args.size());
    for (const auto &arg : process.args) {
        c_args.push_back(arg.c_str());
    }
    c_args.push_back(nullptr);

    std::vector<std::string> envs;
    envs.reserve(process.env.size());
    for (const auto &env : process.env) {
        envs.push_back(env.first + "=" + env.second);
    }
    std::vector<const char *> c_env;
    c_env.reserve(envs.size());
    for (const auto &env : envs) {
        c_env.push_back(env.c_str());
    }
    c_env.push_back(nullptr);

    auto ret = chdir(process.cwd.c_str());
    if (ret != 0) {
        throw std::system_error(errno, std::generic_category(), "chdir");
    }

    ret = setgid(process.uid);
    if (ret != 0) {
        throw std::system_error(errno, std::generic_category(), "setgid");
    }

    if (process.additional_gids) {
        ret = setgroups(process.additional_gids->size(), process.additional_gids->data());
        if (ret != 0) {
            throw std::system_error(errno, std::generic_category(), "setgroups");
        }
    }

    ret = setuid(process.uid);
    if (ret != 0) {
        throw std::system_error(errno, std::generic_category(), "setuid");
    }

    LINYAPS_BOX_DEBUG() << "All opened file describers:\n" << linyaps_box::utils::inspect_fds();

    LINYAPS_BOX_DEBUG() << "Execute container process";

    execvpe(c_args[0],
            const_cast<char *const *>(c_args.data()),
            const_cast<char *const *>(c_env.data()));

    throw std::system_error(errno, std::generic_category(), "execvpe");
}

void wait_create_runtime_result(const linyaps_box::container &container,
                                linyaps_box::utils::file_descriptor &socket)
{
    if (container.get_config().hooks.prestart.empty()
        && container.get_config().hooks.create_runtime.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Request execute createRuntime hooks";

    socket << std::byte(sync_message::REQUEST_CREATERUNTIME_HOOKS);

    LINYAPS_BOX_DEBUG() << "Sync message sent";

    LINYAPS_BOX_DEBUG() << "Wait create runtime result";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message == sync_message::CREATE_RUNTIME_HOOKS_EXECUTED) {
        LINYAPS_BOX_DEBUG() << "Create runtime hooks executed";
        return;
    }
    throw unexpected_sync_message(sync_message::CREATE_RUNTIME_HOOKS_EXECUTED, message);
}

void create_container_hooks(const linyaps_box::container &container,
                            linyaps_box::utils::file_descriptor &socket)
{
    if (container.get_config().hooks.create_container.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Execute create container hooks";

    for (const auto &hook : container.get_config().hooks.create_container) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Create container hooks executed";

    socket << std::byte(sync_message::CREATE_CONTAINER_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void do_pivot_root(const linyaps_box::container &container)
{
    const auto &config = container.get_config();

    auto old_root = linyaps_box::utils::open("/", O_DIRECTORY | O_PATH | O_CLOEXEC);
    auto new_root = linyaps_box::utils::open((container.get_bundle() / config.root.path).c_str(),
                                             O_DIRECTORY | O_PATH | O_CLOEXEC);

    long ret{ -1 };
    ret = mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "mount");
    }

    ret = mount(new_root.proc_path().c_str(),
                new_root.proc_path().c_str(),
                nullptr,
                MS_BIND | MS_REC,
                nullptr);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "mount");
    }

    new_root = linyaps_box::utils::open((container.get_bundle() / config.root.path).c_str(),
                                        O_DIRECTORY | O_PATH | O_CLOEXEC);

    ret = fchdir(new_root.get());
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "fchdir");
    }

    LINYAPS_BOX_DEBUG() << "Pivot root new root: "
                        << linyaps_box::utils::inspect_fd(new_root.get());

    ret = syscall(SYS_pivot_root, ".", ".");
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "pivot_root");
    }

    ret = umount2(".", MNT_DETACH);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "umount2");
    }

    do {
        ret = umount2(".", MNT_DETACH);
        if (ret < 0 && errno == EINVAL) {
            break;
        }
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(), "umount2");
        }
    } while (ret == 0);

    ret = chdir("/");
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "chdir");
    }
}

void start_container_hooks(const linyaps_box::container &container,
                           linyaps_box::utils::file_descriptor &socket)
{
    if (container.get_config().hooks.start_container.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Execute start container hooks";

    for (const auto &hook : container.get_config().hooks.start_container) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Start container hooks executed";

    socket << std::byte(sync_message::START_CONTAINER_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void close_other_fds(const std::set<unsigned int> &except_fds)
{
    LINYAPS_BOX_DEBUG() << "Close all fds excepts " << [&]() {
        std::stringstream ss;
        for (const auto &fd : except_fds) {
            ss << fd << " ";
        }
        return ss.str();
    }();

    auto tmp = except_fds;
    tmp.insert(0);
    tmp.insert(~0U);
    for (auto fd = tmp.begin(); std::next(fd) != tmp.end(); fd++) {
        auto low = *fd + 1;
        auto high = *std::next(fd) - 1;
        if (low >= high) {
            continue;
        }
        LINYAPS_BOX_DEBUG() << "close_range [" << low << ", " << high << "]";
        auto ret = close_range(low, high, 0);
        if (ret != 0) {
            throw std::system_error(errno, std::generic_category(), "close_range");
        }
    }
}

void signal_USR1_handler([[maybe_unused]] int sig)
{
    LINYAPS_BOX_DEBUG() << "Signal USR1 received:" << sig;
}

int clone_fn(void *data) noexcept
try {
    if (getenv("LINYAPS_BOX_CONTAINER_PROCESS_TRACE_ME") != nullptr) {
        auto ret = signal(SIGUSR1, signal_USR1_handler);
        if (ret == SIG_ERR) {
            throw std::system_error(errno, std::generic_category(), "signal");
        }
        assert(ret == SIG_DFL);

        LINYAPS_BOX_INFO()
                << "OCI runtime in container namespace waiting for signal USR1 to continue";
        pause();

        ret = signal(SIGUSR1, SIG_DFL);
        if (ret == SIG_ERR) {
            throw std::system_error(errno, std::generic_category(), "signal");
        }
        assert(ret == signal_USR1_handler);
    }

    LINYAPS_BOX_DEBUG() << "OCI runtime in container namespace starts";

    auto &args = *static_cast<clone_fn_args *>(data);

    assert(args.socket.get() >= 0);
    close_other_fds(
            { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, (unsigned int)(args.socket.get()) });

    const auto &container = *args.container;
    const auto &process = *args.process;
    auto &socket = args.socket;

    configure_container_namespaces(socket);
    configure_mounts(container);
    wait_create_runtime_result(container, socket);
    create_container_hooks(container, socket);
    do_pivot_root(container);
    start_container_hooks(container, socket);
    execute_process(process);
} catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return -1;
} catch (...) {
    std::cerr << "unknown error" << std::endl;
    return -1;
}

} // namespace container_ns

// NOTE: All function in this namespace are running in the runtime namespace.
namespace runtime_ns {

[[nodiscard]] int generate_clone_flag(std::vector<linyaps_box::config::namespace_t> namespaces)
{
    LINYAPS_BOX_DEBUG() << "Generate clone flag from namespaces " << [&]() {
        std::stringstream result;
        result << "[ ";
        for (const auto &ns : namespaces) {
            result << ns.type << " ";
        }
        result << "]";
        return result.str();
    }();

    uint32_t flag = SIGCHLD;
    LINYAPS_BOX_DEBUG() << "Add SIGCHLD, flag=0x" << std::hex << flag;
    uint32_t setted_namespaces = 0;

    for (const auto &ns : namespaces) {
        switch (ns.type) {
        case linyaps_box::config::namespace_t::IPC: {
            flag |= CLONE_NEWIPC;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWIPC, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::UTS: {
            flag |= CLONE_NEWUTS;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWUTS, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::MOUNT: {
            flag |= CLONE_NEWNS;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWNS, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::PID: {
            flag |= CLONE_NEWPID;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWPID, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::NET: {
            flag |= CLONE_NEWNET;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWNET, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::USER: {
            flag |= CLONE_NEWUSER;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWUSER, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::namespace_t::CGROUP: {
            flag |= CLONE_NEWCGROUP;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWCGROUP, flag=0x" << std::hex << flag;
        } break;
        default: {
            throw std::invalid_argument("invalid namespace");
        }
        }

        if ((setted_namespaces & ns.type) != 0) {
            throw std::invalid_argument("duplicate namespace");
        }
        setted_namespaces |= ns.type;
    }

    LINYAPS_BOX_DEBUG() << "Clone flag=0x" << std::hex << flag;

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

        const auto code = errno;
        std::cerr << "munmap: " << strerror(code) << std::endl;
        assert(false);
    }

    [[nodiscard]] void *top() const noexcept
    {
        if constexpr (LINYAPS_BOX_STACK_GROWTH_DOWN) {
            return (char *)this->stack_low + LINYAPS_BOX_CLONE_CHILD_STACK_SIZE;
        } else {
            return (char *)this->stack_low - LINYAPS_BOX_CLONE_CHILD_STACK_SIZE;
        }
    }

private:
    void *stack_low;
};

std::tuple<int, linyaps_box::utils::file_descriptor> start_container_process(
        const linyaps_box::container &container, const linyaps_box::config::process_t &process)
{
    LINYAPS_BOX_DEBUG() << "All opened file describers before socketpair:\n"
                        << linyaps_box::utils::inspect_fds();
    auto sockets = linyaps_box::utils::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    LINYAPS_BOX_DEBUG() << "All opened file describers after socketpair:\n"
                        << linyaps_box::utils::inspect_fds();

    int clone_flag = runtime_ns::generate_clone_flag(container.get_config().namespaces);

    clone_fn_args args = { &container, &process, std::move(sockets.second) };

    LINYAPS_BOX_DEBUG() << "OCI runtime in runtime namespace: PID=" << getpid()
                        << " PIDNS=" << linyaps_box::utils::get_pid_namespace();

    child_stack stack;
    int child_pid = clone(container_ns::clone_fn, stack.top(), clone_flag, (void *)&args);
    if (child_pid < 0) {
        throw std::runtime_error("clone failed");
    }

    if (child_pid == 0) {
        throw std::logic_error("clone should not return in child");
    }

    LINYAPS_BOX_DEBUG() << "OCI runtime in container namespace: PID=" << child_pid
                        << " PIDNS=" << linyaps_box::utils::get_pid_namespace(child_pid);

    return { child_pid, std::move(sockets.first) };
}

void execute_user_namespace_helper(const std::vector<std::string> &args)
{
    LINYAPS_BOX_DEBUG() << "Execute user_namespace helper:" << [&]() {
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
    }();

    auto pid = fork();
    if (pid < 0) {
        throw std::runtime_error("fork failed");
    }

    if (pid == 0) {
        std::vector<const char *> c_args;
        c_args.reserve(args.size());
        for (const auto &arg : args) {
            c_args.push_back(arg.c_str());
        }

        c_args.push_back(nullptr);
        execvp(c_args[0], const_cast<char *const *>(c_args.data()));

        throw std::system_error(errno, std::generic_category(), "execvp");
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

        throw std::system_error(errno, std::generic_category(), "waitpid");
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        LINYAPS_BOX_DEBUG() << "user_namespace helper exited";
        return;
    }

    throw std::runtime_error("user_namespace helper exited abnormally");
}

void configure_gid_mapping(pid_t pid,
                           const std::vector<linyaps_box::config::id_mapping_t> &gid_mappings)
{
    LINYAPS_BOX_DEBUG() << "Configure GID mappings";

    if (gid_mappings.size() == 0) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
        return;
    }

    std::vector<std::string> args;
    args.emplace_back("newgidmap");
    args.push_back(std::to_string(pid));
    for (const auto &mapping : gid_mappings) {
        args.push_back(std::to_string(mapping.container_id));
        args.push_back(std::to_string(mapping.host_id));
        args.push_back(std::to_string(mapping.size));
    }

    execute_user_namespace_helper(args);
}

void configure_uid_mapping(pid_t pid,
                           const std::vector<linyaps_box::config::id_mapping_t> &uid_mappings)
{
    LINYAPS_BOX_DEBUG() << "Configure UID mappings";

    if (uid_mappings.size() == 0) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
        return;
    }

    std::vector<std::string> args;
    args.emplace_back("newuidmap");
    args.push_back(std::to_string(pid));
    for (const auto &mapping : uid_mappings) {
        args.push_back(std::to_string(mapping.container_id));
        args.push_back(std::to_string(mapping.host_id));
        args.push_back(std::to_string(mapping.size));
    }

    execute_user_namespace_helper(args);
}

void configure_container_cgroup([[maybe_unused]] const linyaps_box::container &container)
{
    LINYAPS_BOX_DEBUG() << "Configure container cgroup";
    // TODO: impl
    // enter cgroup -> wait container ready -> enter finalize ->
    // do some other settings -> configuration done
}

void configure_container_namespaces(const linyaps_box::container &container,
                                    linyaps_box::utils::file_descriptor &socket)
{
    LINYAPS_BOX_DEBUG()
            << "Waiting OCI runtime in container namespace to request configure namespace";

    std::byte byte{};
    socket >> byte;
    {
        auto message = sync_message(byte);
        if (message != sync_message::REQUEST_CONFIGURE_NAMESPACE) {
            throw unexpected_sync_message(sync_message::REQUEST_CONFIGURE_NAMESPACE, message);
        }
    }

    LINYAPS_BOX_DEBUG() << "Start configure namespaces";

    const auto &config = container.get_config();

    if (std::find_if(config.namespaces.cbegin(),
                     config.namespaces.cend(),
                     [](const linyaps_box::config::namespace_t &ns) -> bool {
                         return ns.type == linyaps_box::config::namespace_t::USER;
                     })
        != config.namespaces.end()) {

        auto pid = container.status().PID;

        configure_gid_mapping(pid, config.gid_mappings);
        configure_uid_mapping(pid, config.uid_mappings);
    }

    configure_container_cgroup(container);

    LINYAPS_BOX_DEBUG() << "Container namespaces configured";

    byte = std::byte(sync_message::NAMESPACE_CONFIGURED);
    socket << byte;

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void prestart_hooks(const linyaps_box::container &container)
{
    if (container.get_config().hooks.prestart.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Execute prestart hooks";

    for (const auto &hook : container.get_config().hooks.prestart) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Prestart hooks executed";
}

void create_runtime_hooks(const linyaps_box::container &container,
                          linyaps_box::utils::file_descriptor &socket)
{
    if (container.get_config().hooks.prestart.empty()
        && container.get_config().hooks.create_runtime.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Waiting request to execute create runtime hooks";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message != sync_message::REQUEST_CREATERUNTIME_HOOKS) {
        throw unexpected_sync_message(sync_message::REQUEST_CREATERUNTIME_HOOKS, message);
    }

    LINYAPS_BOX_DEBUG() << "Execute prestart hooks";

    for (const auto &hook : container.get_config().hooks.prestart) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Prestart hooks executed";

    LINYAPS_BOX_DEBUG() << "Execute create runtime hooks";

    for (const auto &hook : container.get_config().hooks.create_runtime) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Create runtime hooks executed";

    socket << std::byte(sync_message::CREATE_RUNTIME_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void wait_create_container_result(const linyaps_box::container &container,
                                  linyaps_box::utils::file_descriptor &socket)
{
    if (container.get_config().hooks.create_container.empty()) {
        return;
    }

    LINYAPS_BOX_DEBUG()
            << "Waiting OCI runtime in container namespace send create container hooks result";

    std::byte byte;
    socket >> byte;
    auto message = sync_message(byte);
    if (message == sync_message::CREATE_CONTAINER_HOOKS_EXECUTED) {
        LINYAPS_BOX_DEBUG() << "Create container hooks executed";
        return;
    }
    throw unexpected_sync_message(sync_message::CREATE_CONTAINER_HOOKS_EXECUTED, message);
}

void wait_socket_close(linyaps_box::utils::file_descriptor &socket)
try {
    LINYAPS_BOX_DEBUG() << "All opened file describers:\n" << linyaps_box::utils::inspect_fds();
    std::byte byte;
    LINYAPS_BOX_DEBUG() << "Waiting socket close";
    socket >> byte;
} catch (const linyaps_box::utils::file_descriptor_closed_exception &e) {
    LINYAPS_BOX_DEBUG() << "Socket closed";
    return;
}

void poststart_hooks(const linyaps_box::container &container)
{
    if (container.get_config().hooks.poststart.empty()) {
        return;
    }

    for (const auto &hook : container.get_config().hooks.poststart) {
        execute_hook(hook);
    }
}

void poststop_hooks(const linyaps_box::container &container) noexcept
{
    if (container.get_config().hooks.poststop.empty()) {
        return;
    }

    for (const auto &hook : container.get_config().hooks.poststart) {
        try {
            execute_hook(hook);
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

[[nodiscard]] int wait_container_process(pid_t pid)
{
    int status = 0;

    pid_t ret = -1;
    while (ret == -1) {
        ret = waitpid(pid, &status, 0);

        if (ret != -1) {
            break;
        }

        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }

        throw std::system_error(errno, std::generic_category(), "waitpid");
    }
    return WEXITSTATUS(status);
}

} // namespace runtime_ns

} // namespace

linyaps_box::container::container(std::shared_ptr<status_directory> status_dir,
                                  const std::string &id,
                                  const std::filesystem::path &bundle,
                                  std::filesystem::path config,
                                  cgroup_manager_t manager)
    : container_ref(std::move(status_dir), id)
    , bundle(bundle)
{
    if (config.is_relative()) {
        config = std::filesystem::canonical(bundle / config);
    }

    std::ifstream ifs(config);
    LINYAPS_BOX_DEBUG() << "load config from " << config;
    this->config = linyaps_box::config::parse(ifs);

    {
        container_status_t status;
        status.ID = id;
        status.PID = getpid();
        status.status = container_status_t::runtime_status::CREATING;
        status.bundle = bundle;
        status.created = ""; // FIXME
        status.owner = getuid();
        this->status_dir().write(status);
    }

    switch (manager) {
    case cgroup_manager_t::disabled: {
        this->manager = std::make_unique<disabled_cgroup_manager>();
    } break;
    case cgroup_manager_t::systemd:
    case cgroup_manager_t::cgroupfs:
        throw std::runtime_error("unsupported cgroup manager");
    }
}

const linyaps_box::config &linyaps_box::container::get_config() const
{
    return this->config;
}

const std::filesystem::path &linyaps_box::container::get_bundle() const
{
    return this->bundle;
}

// maybe we need a internal run function?
int linyaps_box::container::run(const config::process_t &process)
{
    int container_process_exit_code{ -1 };
    try {
        // TODO: there are some thing that should be done before starting the container process
        // e.g. do something before creating cgroup by selecting manager, selinux label, seccomp
        // setup, etc.

        // TODO: cgroup preenter
        auto [child_pid, socket] = runtime_ns::start_container_process(*this, process);

        {
            auto status = this->status();
            assert(status.status == container_status_t::runtime_status::CREATING);
            status.PID = child_pid;
            status.status = container_status_t::runtime_status::CREATED;
            this->status_dir().write(status);
        }

        runtime_ns::configure_container_namespaces(*this, socket);
        runtime_ns::prestart_hooks(*this);
        runtime_ns::create_runtime_hooks(*this, socket);
        runtime_ns::wait_create_container_result(*this, socket);
        runtime_ns::wait_socket_close(socket);
        runtime_ns::poststart_hooks(*this);

        // TODO: support detach from the parent's process
        // Now we wait for the container process to exit
        container_process_exit_code = runtime_ns::wait_container_process(this->status().PID);

        {
            auto status = this->status();
            assert(status.status == container_status_t::runtime_status::STOPPED);
            status.PID = child_pid;
            this->status_dir().write(status);
        }

        runtime_ns::poststop_hooks(*this);
    } catch (const std::exception &e) {
        LINYAPS_BOX_ERR() << "failed to run a container: " << e.what();
    }

    this->status_dir().remove(this->get_id());

    // TODO: cleanup cgroup

    return container_process_exit_code;
}

void linyaps_box::container::cgroup_preenter(const cgroup_options &options,
                                             utils::file_descriptor &dirfd)
{
    auto type = utils::get_cgroup_type();
    if (type != utils::cgroup_t::unified) {
        return;
    }

    this->manager->precreate_cgroup(options, dirfd);
}
