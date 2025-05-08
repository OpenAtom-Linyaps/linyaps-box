// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container.h"

#include "linyaps_box/impl/disabled_cgroup_manager.h"
#include "linyaps_box/utils/cgroups.h"
#include "linyaps_box/utils/close_range.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/fstat.h"
#include "linyaps_box/utils/inspect.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/mkdir.h"
#include "linyaps_box/utils/mknod.h"
#include "linyaps_box/utils/open_file.h"
#include "linyaps_box/utils/platform.h"
#include "linyaps_box/utils/socketpair.h"
#include "linyaps_box/utils/touch.h"

#include <linux/magic.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/statfs.h>
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
#include <pwd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef LINYAPS_BOX_ENABLE_CAP
#include <sys/capability.h>
#endif

constexpr auto propagations_flag = (MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE);

namespace linyaps_box {

struct container_data
{
    bool deny_setgroups{ false };
};

container_data &get_private_data(const linyaps_box::container &c) noexcept
{
    return *(c.data);
}

} // namespace linyaps_box

namespace {

enum class sync_message : std::uint8_t {
    REQUEST_CONFIGURE_NAMESPACE,
    NAMESPACE_CONFIGURED,
    REQUEST_PRESTART_HOOKS,
    PRESTART_HOOKS_EXECUTED,
    REQUEST_CREATERUNTIME_HOOKS,
    CREATERUNTIME_HOOKS_EXECUTED,
    CREATECONTAINER_HOOKS_EXECUTED,
    STARTCONTAINER_HOOKS_EXECUTED,
};

struct security_status
{
    unsigned long cap{ 0 };
};

std::ostream &operator<<(std::ostream &os, const sync_message message)
{
    switch (message) {
    case sync_message::REQUEST_CONFIGURE_NAMESPACE: {
        os << "REQUEST_CONFIGURE_NAMESPACE";
    } break;
    case sync_message::NAMESPACE_CONFIGURED: {
        os << "NAMESPACE_CONFIGURED";
    } break;
    case sync_message::REQUEST_PRESTART_HOOKS: {
        os << "REQUEST_PRESTART_HOOKS";
    } break;
    case sync_message::PRESTART_HOOKS_EXECUTED: {
        os << "PRESTART_HOOKS_EXECUTED";
    } break;
    case sync_message::REQUEST_CREATERUNTIME_HOOKS: {
        os << "REQUEST_PRESTART_AND_CREATERUNTIME_HOOKS";
    } break;
    case sync_message::CREATERUNTIME_HOOKS_EXECUTED: {
        os << "CREATE_RUNTIME_HOOKS_EXECUTED";
    } break;
    case sync_message::CREATECONTAINER_HOOKS_EXECUTED: {
        os << "CREATE_CONTAINER_HOOKS_EXECUTED";
    } break;
    case sync_message::STARTCONTAINER_HOOKS_EXECUTED: {
        os << "START_CONTAINER_HOOKS_EXECUTED";
    } break;
    default: {
        assert(false);
        os << "UNKNOWN " << (uint8_t)message;
    } break;
    }
    return os;
}

std::string dump_mount_flags(uint flags) noexcept
{
    std::stringstream ss;
    ss << "[ ";
    if ((flags & MS_RDONLY) != 0) {
        ss << "MS_RDONLY ";
    }
    if ((flags & MS_NOSUID) != 0) {
        ss << "MS_NOSUID ";
    }
    if ((flags & MS_NODEV) != 0) {
        ss << "MS_NODEV ";
    }
    if ((flags & MS_NOEXEC) != 0) {
        ss << "MS_NOEXEC ";
    }
    if ((flags & MS_SYNCHRONOUS) != 0) {
        ss << "MS_SYNCHRONOUS ";
    }
    if ((flags & MS_REMOUNT) != 0) {
        ss << "MS_REMOUNT ";
    }
    if ((flags & MS_MANDLOCK) != 0) {
        ss << "MS_MANDLOCK ";
    }
    if ((flags & MS_DIRSYNC) != 0) {
        ss << "MS_DIRSYNC ";
    }
    if ((flags & LINGYAPS_MS_NOSYMFOLLOW) != 0) {
        ss << "MS_NOSYMFOLLOW ";
    }
    if ((flags & MS_NOATIME) != 0) {
        ss << "MS_NOATIME ";
    }
    if ((flags & MS_NODIRATIME) != 0) {
        ss << "MS_NODIRATIME ";
    }
    if ((flags & MS_BIND) != 0) {
        ss << "MS_BIND ";
    }
    if ((flags & MS_MOVE) != 0) {
        ss << "MS_MOVE ";
    }
    if ((flags & MS_REC) != 0) {
        ss << "MS_REC ";
    }
    if ((flags & MS_SILENT) != 0) {
        ss << "MS_SILENT ";
    }
    if ((flags & MS_POSIXACL) != 0) {
        ss << "MS_POSIXACL ";
    }
    if ((flags & MS_UNBINDABLE) != 0) {
        ss << "MS_UNBINDABLE ";
    }
    if ((flags & MS_PRIVATE) != 0) {
        ss << "MS_PRIVATE ";
    }
    if ((flags & MS_SLAVE) != 0) {
        ss << "MS_SLAVE ";
    }
    if ((flags & MS_SHARED) != 0) {
        ss << "MS_SHARED ";
    }
    if ((flags & MS_RELATIME) != 0) {
        ss << "MS_RELATIME ";
    }
    if ((flags & MS_KERNMOUNT) != 0) {
        ss << "MS_KERNMOUNT ";
    }
    if ((flags & MS_I_VERSION) != 0) {
        ss << "MS_I_VERSION ";
    }
    if ((flags & MS_STRICTATIME) != 0) {
        ss << "MS_STRICTATIME ";
    }
    if ((flags & MS_LAZYTIME) != 0) {
        ss << "MS_LAZYTIME ";
    }
    if ((flags & MS_ACTIVE) != 0) {
        ss << "MS_ACTIVE ";
    }
    if ((flags & MS_NOUSER) != 0) {
        ss << "MS_NOUSER ";
    }
    ss << "]";
    return ss.str();
}

class unexpected_sync_message : public std::logic_error
{
public:
    unexpected_sync_message(sync_message excepted, sync_message actual)
        : std::logic_error([excepted, actual]() -> std::string {
            std::stringstream stream;
            stream << "unexpected sync message: expected " << excepted << " got " << actual;
            return std::move(stream).str();
        }())
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
        const auto *bin = hook.path.c_str();
        std::vector<const char *> c_args{ nullptr };
        if (hook.args) {
            c_args.reserve(hook.args.value().size() + 1);
            const auto &args = hook.args.value();
            std::for_each(args.crbegin(), args.crend(), [&c_args](const std::string &arg) {
                c_args.insert(c_args.begin(), arg.c_str());
            });
        }

        std::vector<const char *> c_env{ nullptr };
        if (hook.env) {
            std::vector<std::string> envs;
            envs.reserve(hook.env.value().size() + 1);
            for (const auto &[key, value] : hook.env.value()) {
                std::string tmp{ key };
                tmp.append('+' + value);
                envs.push_back(std::move(tmp));
            }

            c_env.reserve(envs.size() + 1);
            std::for_each(envs.crbegin(), envs.crend(), [&c_env](const std::string &env) {
                c_env.insert(c_env.begin(), env.c_str());
            });
        }

        execvpe(bin,
                const_cast<char *const *>(c_args.data()), // NOLINT
                const_cast<char *const *>(c_env.data())); // NOLINT

        std::cerr << "execvp: " << strerror(errno) << " errno=" << errno << std::endl;
        exit(1);
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

        throw std::system_error(errno, std::generic_category(), "waitpid " + std::to_string(pid));
    }

    if (WIFEXITED(status)) {
        return;
    }

    std::stringstream stream;
    stream << "hook terminated by signal" << WTERMSIG(status) << " with " << WEXITSTATUS(status);
    throw std::runtime_error(std::move(stream).str());
}

struct clone_fn_args
{
    const linyaps_box::container *container;
    const linyaps_box::config::process_t *process;
    linyaps_box::utils::file_descriptor socket;
};

// NOTE: All function in this namespace are running in the container namespace.
namespace container_ns {

void initialize_container(const linyaps_box::config &config,
                          linyaps_box::utils::file_descriptor &socket)
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

    if (config.process.oom_score_adj) {
        auto score = std::to_string(config.process.oom_score_adj.value());
        LINYAPS_BOX_DEBUG() << "Set oom score to " << score;

        std::ofstream ofs("/proc/self/oom_score_adj");
        if (!ofs) {
            throw std::runtime_error("failed to open /proc/self/oom_score_adj");
        }

        ofs << score;
    }
}

void syscall_mount(const char *_special_file,
                   const char *_dir,
                   const char *_fstype,
                   unsigned long int _rwflag,
                   const void *_data)
{
    constexpr decltype(auto) fd_prefix = "/proc/self/fd/";
    LINYAPS_BOX_DEBUG() << "mount\n"
                        << "\t_special_file = " << [_special_file]() -> std::string {
        if (_special_file == nullptr) {
            return "nullptr";
        }
        if (auto str = std::string_view{ _special_file }; str.rfind(fd_prefix, 0) == 0) {
            return linyaps_box::utils::inspect_fd(std::stoi(str.data() + sizeof(fd_prefix) - 1));
        }
        return _special_file;
    }() << "\n\t_dir = "
        << [_dir]() -> std::string {
        if (_dir == nullptr) {
            return "nullptr";
        }
        if (auto str = std::string_view{ _dir }; str.rfind(fd_prefix, 0) == 0) {
            return linyaps_box::utils::inspect_fd(std::stoi(str.data() + sizeof(fd_prefix) - 1));
        }
        return _dir;
    }() << "\n\t_fstype = "
        << [_fstype]() -> std::string {
        if (_fstype == nullptr) {
            return "nullptr";
        }
        return _fstype;
    }() << "\n\t_rwflag = "
        << dump_mount_flags(_rwflag) << "\n\t_data = " << [_data]() -> std::string {
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

struct remount_t
{
    linyaps_box::utils::file_descriptor destination_fd;
    unsigned long flags{};
    std::string data;
};

void do_remount(const remount_t &mount)
{
    assert(mount.destination_fd.get() != -1);
    assert(mount.flags & (MS_BIND | MS_REMOUNT | MS_RDONLY));

    auto destination = mount.destination_fd.proc_path();
    const auto *data_ptr = mount.data.empty() ? nullptr : mount.data.c_str();

    // for old kernel
    if ((mount.flags & (MS_REMOUNT | MS_RDONLY)) != 0) {
        data_ptr = nullptr;
    }

    LINYAPS_BOX_DEBUG() << "Remount " << destination << " with flags "
                        << dump_mount_flags(mount.flags);
    try {
        syscall_mount(nullptr, destination.c_str(), nullptr, mount.flags, data_ptr);
        return;
    } catch (const std::system_error &e) {
        LINYAPS_BOX_DEBUG() << "Failed to remount "
                            << linyaps_box::utils::inspect_path(mount.destination_fd.get())
                            << "with flags " << dump_mount_flags(mount.flags) << ": " << e.what()
                            << ", retrying";
    }

    auto state = linyaps_box::utils::statfs(mount.destination_fd);
    auto remount_flags = state.f_flags & (MS_NOSUID | MS_NODEV | MS_NOEXEC);
    if ((remount_flags | mount.flags) != mount.flags) {
        try {
            syscall_mount(nullptr,
                          destination.c_str(),
                          nullptr,
                          remount_flags | mount.flags,
                          data_ptr);
            return;
        } catch (const std::system_error &e) {
            LINYAPS_BOX_DEBUG() << "Failed to remount "
                                << linyaps_box::utils::inspect_path(mount.destination_fd.get())
                                << "with flags " << dump_mount_flags(remount_flags | mount.flags)
                                << ": " << e.what() << ", retrying";
        }
    }

    if ((state.f_flags & MS_RDONLY) != 0) {
        remount_flags = state.f_flags & (MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY);
        syscall_mount(nullptr, destination.c_str(), nullptr, mount.flags | remount_flags, data_ptr);
    }
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

    if (errno != EEXIST) {
        throw std::system_error(errno, std::system_category(), "symlinkat");
    }

    auto stat = linyaps_box::utils::lstatat(root, destination);
    if (!S_ISLNK(stat.st_mode)) {
        throw std::system_error(errno,
                                std::system_category(),
                                "destination " + destination.string()
                                        + " already exists and is not a symlink");
    }

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

[[nodiscard]] linyaps_box::utils::file_descriptor
ensure_mount_destination(bool isDir,
                         const linyaps_box::utils::file_descriptor &root,
                         const linyaps_box::config::mount_t &mount)
try {
    assert(mount.destination.has_value());
    auto open_flag = O_PATH;
    if ((mount.flags & LINGYAPS_MS_NOSYMFOLLOW) != 0) {
        open_flag |= O_NOFOLLOW;
    }

    return linyaps_box::utils::open_at(root, mount.destination.value(), open_flag);
} catch (const std::system_error &e) {
    if (e.code().value() != ENOENT) {
        throw;
    }

    auto path = mount.destination.value();
    LINYAPS_BOX_DEBUG() << "Destination " << (isDir ? "directory " : "file ") << path
                        << " not exists: " << e.what();

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

    syscall_mount(nullptr, destination.proc_path().c_str(), nullptr, flags, nullptr);
}

[[nodiscard]] linyaps_box::utils::file_descriptor do_bind_mount(
        const linyaps_box::utils::file_descriptor &root, const linyaps_box::config::mount_t &mount)
{
    assert(mount.flags & MS_BIND);
    assert(mount.source.has_value());
    assert(mount.destination.has_value());

    auto open_flag = O_PATH;
    if ((mount.flags & LINGYAPS_MS_NOSYMFOLLOW) != 0) {
        open_flag |= O_NOFOLLOW;
    }
    auto source_fd = linyaps_box::utils::open(mount.source.value(), open_flag);
    auto source_stat = linyaps_box::utils::lstatat(source_fd, "");

    auto sourceIsDir = S_ISDIR(source_stat.st_mode);
    auto destination_fd = ensure_mount_destination(sourceIsDir, root, mount);
    auto bind_flags = mount.flags & ~(propagations_flag | MS_RDONLY);

    try {
        // bind mount will ignore fstype and data
        syscall_mount(source_fd.proc_path().c_str(),
                      destination_fd.proc_path().c_str(),
                      nullptr,
                      bind_flags,
                      nullptr);
    } catch (const std::system_error &e) {
        // mounting sysfs with rootless/userns container will fail with EPERM
        // TODO: try to bind mount /sys
        throw;
    }

    return linyaps_box::utils::open_at(root, mount.destination.value(), open_flag);
}

void do_cgroup_mount([[maybe_unused]] const linyaps_box::utils::file_descriptor &root,
                     [[maybe_unused]] const linyaps_box::config::mount_t &mount,
                     [[maybe_unused]] std::string_view unified_cgroup_path)
{
    // TODO: implement
    throw std::runtime_error("mount cgroup: Not implemented");
}

[[nodiscard]] std::optional<remount_t> do_mount(const linyaps_box::container &container,
                                                const linyaps_box::utils::file_descriptor &root,
                                                const linyaps_box::config::mount_t &mount)
{
    // FIXME: this is a workaround, it should be fixed in the future
    static auto is_sys_rbind{ false };

    LINYAPS_BOX_DEBUG() << "Mount " << [&]() -> std::string {
        std::stringstream result;
        if (!mount.type.empty()) {
            result << mount.type << ":";
        }
        result << mount.source.value_or("none");
        return result.str();
    }() << " to "
        << mount.destination.value().string();

    if (mount.type.rfind("cgroup", 0) != std::string::npos) {
        // if /sys is bind mount recursively, then skip /sys/fs/cgroup
        const auto &linux = container.get_config().linux;
        if (linux && linux->namespaces) {
            const auto &namespaces = linux->namespaces;
            auto unshared_cgroup_ns = std::find_if(
                    namespaces->cbegin(),
                    namespaces->cend(),
                    [](const linyaps_box::config::linux_t::namespace_t &ns) -> bool {
                        return ns.type == linyaps_box::config::linux_t::namespace_t::type_t::CGROUP;
                    });
            if (mount.destination == "/sys/fs/cgroup" && is_sys_rbind) {
                if (unshared_cgroup_ns != namespaces->cend()) {
                    throw std::runtime_error("unshared cgroup namespace is not supported");
                }

                return std::nullopt;
            }
        }

        do_cgroup_mount(root, mount, "");
        return std::nullopt;
    }

    // TODO: fstype == sysfs and it's fallback
    linyaps_box::utils::file_descriptor destination_fd;
    if ((mount.flags & MS_BIND) != 0) {
        destination_fd = do_bind_mount(root, mount);
        if (!is_sys_rbind && mount.destination == "/sys" && (mount.flags & MS_REC) != 0) {
            is_sys_rbind = true;
        }
    } else {
        // mount other types
        destination_fd = ensure_mount_destination(true, root, mount);
        syscall_mount(mount.source ? mount.source.value().c_str() : nullptr,
                      destination_fd.proc_path().c_str(),
                      mount.type.empty() ? nullptr : mount.type.c_str(),
                      mount.flags,
                      mount.data.empty() ? nullptr : mount.data.c_str());
    }

    do_propagation_mount(destination_fd, mount.propagation_flags);

    bool need_remount{ false };
    if ((mount.flags & (MS_RDONLY | MS_BIND)) != 0) {
        need_remount = true;
    }

    // procfs mount option only working with remount (e.g. hidepid=2)
    // this limitation is no longer required after kernel 5.7
    // we do this for compatible with older kernel
    // https://web.git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=69879c01a0c3f70e0887cfb4d9ff439814361e46
    if (!mount.data.empty() && mount.type == "proc") {
        need_remount = true;
    }

    if (!need_remount) {
        LINYAPS_BOX_DEBUG() << "no need to remount";
        return std::nullopt;
    }

    auto remount_flags = mount.flags | MS_REMOUNT;
    if (mount.type != "proc") {
        remount_flags |= MS_BIND;
    }

    auto delay_readonly_mount = remount_t{ std::move(destination_fd), remount_flags, mount.data };
    if ((remount_flags & MS_RDONLY) == 0) {
        // if not readonly mount, just remount directly
        LINYAPS_BOX_DEBUG() << "remount " << mount.destination.value() << " directly";
        do_remount(delay_readonly_mount);
        return std::nullopt;
    }

    LINYAPS_BOX_DEBUG() << "remount delayed";
    return delay_readonly_mount;
}

class mounter
{
    void make_rootfs_private()
    {
        auto rootfsfd = root.duplicate();
        const auto &rootfs = rootfsfd.current_path();
        for (auto it = std::cbegin(rootfs); it != std::cend(rootfs); ++it) {
            LINYAPS_BOX_DEBUG() << "make " << rootfsfd.current_path() << " private";

            try {
                do_propagation_mount(rootfsfd, MS_PRIVATE);
                return;
            } catch (const std::system_error &e) {
                auto parent_fd = ::openat(rootfsfd.get(), "..", O_PATH | O_CLOEXEC);
                if (parent_fd < 0) {
                    throw std::system_error(errno,
                                            std::generic_category(),
                                            "openat: failed to open "
                                                    + rootfsfd.current_path().string() + "/..");
                }

                rootfsfd = linyaps_box::utils::file_descriptor(parent_fd);
            }
        }

        throw std::runtime_error("make rootfs private failed");
    }

public:
    explicit mounter(linyaps_box::utils::file_descriptor root,
                     const linyaps_box::container &container)
        : container(container)
        , root(std::move(root))
    {
    }

    void configure_rootfs()
    {
        const auto &config = container.get_config();

        if (!config.linux || !config.linux->namespaces) {
            return;
        }

        auto unshared_mount_ns =
                std::find_if(std::cbegin(*(config.linux->namespaces)),
                             std::cend(*(config.linux->namespaces)),
                             [](const linyaps_box::config::linux_t::namespace_t &ns) -> bool {
                                 return ns.type == linyaps_box::config::linux_t::namespace_t::MOUNT;
                             });
        if (unshared_mount_ns == std::cend(*(config.linux->namespaces))) {
            LINYAPS_BOX_DEBUG() << "no unshared mount namespace";
            return;
        }

        // we will pivot root later
        LINYAPS_BOX_DEBUG() << "configure rootfs";
        do_propagation_mount(linyaps_box::utils::open("/", O_PATH | O_CLOEXEC | O_DIRECTORY),
                             MS_REC | MS_PRIVATE);

        // make sure the parent mount of rootfs is private
        // pivot root will fail if it has shared propagation type
        make_rootfs_private();

        LINYAPS_BOX_DEBUG() << "rebind container rootfs";

        linyaps_box::config::mount_t mount;
        mount.source = root.current_path();
        mount.destination = ".";
        mount.flags = MS_BIND | MS_REC;
        // mount.propagation_flags = MS_PRIVATE | MS_REC;
        auto ret = do_mount(container, root, mount);
        assert(!ret);

        // reopen rootfs after mount
        root = linyaps_box::utils::open(root.current_path(), O_PATH | O_CLOEXEC | O_DIRECTORY);

        if (config.root.readonly) {
            LINYAPS_BOX_DEBUG() << "remount bind rootfs to readonly";
            remount_t mount;
            mount.destination_fd = root.duplicate();
            mount.flags = MS_RDONLY | MS_BIND | MS_REMOUNT;
            remounts.push_back(std::move(mount));
        }
    }

    void do_mounts()
    {
        for (const auto &mount : container.get_config().mounts) {
            this->mount(mount);
        }
    }

    void mount(const linyaps_box::config::mount_t &mount)
    {
        if ((mount.extra_flags & linyaps_box::config::mount_t::COPY_SYMLINK) != 0) {
            auto ret = create_destination_symlink(root,
                                                  mount.source.value(),
                                                  mount.destination.value());
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
        const auto &linux = container.get_config().linux;
        if (!linux || !linux->readonly_paths) {
            LINYAPS_BOX_DEBUG() << "no readonly paths";
            return;
        }

        for (const auto &path : *linux->readonly_paths) {
            linyaps_box::utils::file_descriptor dst;
            try {
                dst = linyaps_box::utils::open_at(root, path);
            } catch (const std::system_error &e) {
                if (auto err = e.code().value(); err == ENOENT || err == EACCES) {
                    continue;
                }

                throw;
            }

            auto mount_flag = MS_BIND | MS_PRIVATE | MS_RDONLY | MS_REC;

            // readonly path is an absolute path within the container,
            // the path is already exists in the container when making it readonly
            // so we should inherit the mount flags to keep it as same as the original
            auto ret = linyaps_box::utils::statfs(dst);
            mount_flag |= ret.f_flags;

            // parent mount flags may contain MS_REMOUNT, we should remove it due to the
            // readonly path is not mounted yet
            mount_flag &= ~MS_REMOUNT;

            linyaps_box::config::mount_t mount{};
            mount.destination = path;
            mount.source = dst.proc_path();
            mount.flags = mount_flag;

            LINYAPS_BOX_DEBUG() << "make readonly path " << path << " with "
                                << dump_mount_flags(mount.flags);
            auto delay_mount = do_mount(container, root, mount);
            // we do rbind for this path, so remount will be done after finalize
            assert(delay_mount);
            remounts.emplace_back(std::move(delay_mount).value());
        }
    }

    void make_path_masked()
    {
        const auto &linux = container.get_config().linux;
        if (!linux || !linux->masked_paths) {
            LINYAPS_BOX_DEBUG() << "no masked paths";
            return;
        }

        for (const auto &path : *linux->masked_paths) {
            linyaps_box::utils::file_descriptor dst;
            try {
                dst = linyaps_box::utils::open_at(root, path);
            } catch (const std::system_error &e) {
                if (auto err = e.code().value(); err == ENOENT || err == EACCES) {
                    continue;
                }

                throw;
            }

            auto ret = linyaps_box::utils::fstatat(dst, "");
            auto mount = linyaps_box::config::mount_t{};

            mount.destination = path;
            mount.flags = MS_RDONLY;

            if (S_ISDIR(ret.st_mode)) {
                mount.source = "tmpfs";
                mount.type = "tmpfs";
                mount.data = "size=0k";

                LINYAPS_BOX_DEBUG() << "mask directory " << path;
                auto delay_mount = do_mount(container, root, mount);
                assert(delay_mount);
                remounts.emplace_back(std::move(delay_mount).value());
                continue;
            }

            mount.source = "/dev/null";
            mount.flags |= MS_BIND;

            LINYAPS_BOX_DEBUG() << "mask file " << path;
            auto delay_mount = do_mount(container, root, mount);
            assert(delay_mount);
            remounts.emplace_back(std::move(delay_mount).value());
        }
    }

    void finalize()
    {
        this->configure_default_filesystems();
        this->configure_default_devices();

        LINYAPS_BOX_DEBUG() << "finalize " << remounts.size() << " remounts";
        // our mount process has to do with the order
        // the last mount should be the last remount
        std::for_each(remounts.crbegin(), remounts.crend(), do_remount);
    }

private:
    const linyaps_box::container &container;
    linyaps_box::utils::file_descriptor root;
    std::vector<remount_t> remounts;

    // https://github.com/opencontainers/runtime-spec/blob/09fcb39bb7185b46dfb206bc8f3fea914c674779/config-linux.md#default-filesystems
    void configure_default_filesystems()
    {
        LINYAPS_BOX_DEBUG() << "configure default filesystems";

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
            mount.flags = MS_NOSUID | MS_STRICTATIME;
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

    void configure_device(const std::filesystem::path &destination,
                          mode_t mode,
                          int type,
                          dev_t dev,
                          uid_t uid,
                          gid_t gid)
    {
        assert(destination.is_absolute());

        if (type != S_IFCHR && type != S_IFBLK && type != S_IFIFO) {
            throw std::runtime_error("unsupported device type");
        }

        std::optional<linyaps_box::utils::file_descriptor> destination_fd;
        try {
            destination_fd = linyaps_box::utils::open_at(root, destination.relative_path(), O_PATH);
        } catch (const std::system_error &e) {
            if (e.code().value() != ENOENT) {
                throw;
            }
        }

        if (destination_fd.has_value()) {
            // if already exists, check if it is a required device
            auto stat = linyaps_box::utils::lstatat(*destination_fd, "");
            bool satisfied{ true };
            if (__S_ISTYPE(stat.st_mode, type)) {
                auto dump_mode = [](mode_t mode) {
                    if (S_ISCHR(mode)) {
                        return "Character";
                    }

                    if (S_ISBLK(mode)) {
                        return "Block";
                    }

                    if (S_ISFIFO(mode)) {
                        return "FIFO";
                    }

                    return "unknown";
                };

                LINYAPS_BOX_DEBUG()
                        << "the type of existing device: " << destination << " is not required\n"
                        << "expect " << dump_mode(mode) << ", got " << dump_mode(stat.st_mode);
                satisfied = false;
            }

            if (major(stat.st_dev) != major(dev) || minor(stat.st_dev) != minor(dev)) {
                LINYAPS_BOX_DEBUG()
                        << "the kind of existing device: " << destination << " is not required\n"
                        << "expect " << major(dev) << ":" << minor(dev) << ", got "
                        << major(stat.st_dev) << ":" << minor(stat.st_dev);
                satisfied = false;
            }

            if (stat.st_uid != uid || stat.st_gid != gid) {
                LINYAPS_BOX_DEBUG()
                        << "the owner of existing device: " << destination << " is not required\n"
                        << "expect " << uid << ":" << gid << ", got " << stat.st_uid << ":"
                        << stat.st_gid;
                satisfied = false;
            }

            if (satisfied) {
                return;
            }

            throw std::runtime_error(destination.string()
                                     + " already exists but it's not satisfied with requirement");
        }

        try {
            auto path = destination.relative_path();
            linyaps_box::utils::mknodat(root, path, mode | type, dev);

            auto new_dev = linyaps_box::utils::open_at(root, path, O_PATH);
            path = new_dev.proc_path();
            if (chmod(path.c_str(), mode) < 0) {
                throw std::system_error(errno, std::generic_category(), "chmod");
            }

            if (chown(path.c_str(), uid, gid) < 0) {
                throw std::system_error(errno, std::generic_category(), "chown");
            }
        } catch (const std::system_error &e) {
            if (e.code().value() != EPERM) {
                throw;
            }
        }

        // NOTE: fallback to bind mount host device into container
        LINYAPS_BOX_DEBUG() << "fallback to bind mount device";
        linyaps_box::config::mount_t mount;
        mount.source = destination;
        mount.destination = destination;
        mount.type = "bind";
        mount.flags = MS_BIND | MS_PRIVATE | MS_NOEXEC | MS_NOSUID;
        this->mount(mount);
    }

    void configure_default_devices()
    {
        LINYAPS_BOX_DEBUG() << "Configure default devices";

        constexpr auto default_mode = 0666;
        auto uid = container.get_config().process.user.uid;
        auto gid = container.get_config().process.user.gid;
        this->configure_device("/dev/null", default_mode, S_IFCHR, makedev(1, 3), uid, gid);
        this->configure_device("/dev/zero", default_mode, S_IFCHR, makedev(1, 5), uid, gid);
        this->configure_device("/dev/full", default_mode, S_IFCHR, makedev(1, 7), uid, gid);
        this->configure_device("/dev/random", default_mode, S_IFCHR, makedev(1, 8), uid, gid);
        this->configure_device("/dev/urandom", default_mode, S_IFCHR, makedev(1, 9), uid, gid);
        this->configure_device("/dev/tty", default_mode, S_IFCHR, makedev(5, 0), uid, gid);

        // TODO Handle `/dev/console`;

        // TODO Handle `/dev/ptmx`;
    }
};

void configure_mounts(const linyaps_box::container &container, const std::filesystem::path &rootfs)
{
    LINYAPS_BOX_DEBUG() << "Configure mounts";

    const auto &config = container.get_config();

    if (config.mounts.empty()) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
        return;
    }

    auto m = std::make_unique<mounter>(
            linyaps_box::utils::open(rootfs, O_PATH | O_DIRECTORY | O_CLOEXEC),
            container);

    // TODO: if root is read only, add it to remount list

    LINYAPS_BOX_DEBUG() << "Processing mount points";

    m->configure_rootfs();
    m->do_mounts();
    m->make_path_masked();
    m->make_path_readonly();
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

    ret = setgid(process.user.gid);
    if (ret != 0) {
        throw std::system_error(errno, std::generic_category(), "setgid");
    }

    if (process.user.additional_gids) {
        ret = setgroups(process.user.additional_gids->size(), process.user.additional_gids->data());
        if (ret != 0) {
            throw std::system_error(errno, std::generic_category(), "setgroups");
        }
    }

    ret = setuid(process.user.uid);
    if (ret != 0) {
        throw std::system_error(errno, std::generic_category(), "setuid");
    }

    LINYAPS_BOX_DEBUG() << "All opened file describers:\n" << linyaps_box::utils::inspect_fds();

    LINYAPS_BOX_DEBUG() << "Execute container process:" << [&process]() -> std::string {
        std::stringstream ss;
        ss << " " << process.args[0];
        for (size_t i = 1; i < process.args.size(); ++i) {
            ss << " " << process.args[i];
        }
        return ss.str();
    }();

    execvpe(c_args[0],
            const_cast<char *const *>(c_args.data()),
            const_cast<char *const *>(c_env.data()));

    throw std::system_error(errno, std::generic_category(), "execvpe");
}

void wait_prestart_hooks_result(const linyaps_box::container &container,
                                linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.prestart) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Request execute prestart hooks";

    socket << std::byte(sync_message::REQUEST_PRESTART_HOOKS);

    LINYAPS_BOX_DEBUG() << "Sync message sent";

    LINYAPS_BOX_DEBUG() << "Wait prestart runtime result";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message == sync_message::PRESTART_HOOKS_EXECUTED) {
        LINYAPS_BOX_DEBUG() << "Prestart hooks executed";
        return;
    }

    throw unexpected_sync_message(sync_message::PRESTART_HOOKS_EXECUTED, message);
}

void wait_create_runtime_result(const linyaps_box::container &container,
                                linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.create_runtime) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Request execute createRuntime hooks";

    socket << std::byte(sync_message::REQUEST_CREATERUNTIME_HOOKS);

    LINYAPS_BOX_DEBUG() << "Sync message sent";

    LINYAPS_BOX_DEBUG() << "Wait create runtime result";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message == sync_message::CREATERUNTIME_HOOKS_EXECUTED) {
        LINYAPS_BOX_DEBUG() << "Create runtime hooks executed";
        return;
    }

    throw unexpected_sync_message(sync_message::CREATERUNTIME_HOOKS_EXECUTED, message);
}

void create_container_hooks(const linyaps_box::container &container,
                            linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.create_container) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Execute create container hooks";

    for (const auto &hook : container.get_config().hooks.create_container.value()) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Create container hooks executed";

    socket << std::byte(sync_message::CREATECONTAINER_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void do_pivot_root(const std::filesystem::path &rootfs)
{
    LINYAPS_BOX_DEBUG() << "start pivot root";
    LINYAPS_BOX_DEBUG() << linyaps_box::utils::inspect_fds();
    auto old_root = linyaps_box::utils::open("/", O_DIRECTORY | O_PATH | O_CLOEXEC);
    auto new_root = linyaps_box::utils::open(rootfs.c_str(), O_DIRECTORY | O_RDONLY | O_CLOEXEC);

    auto old_root_stat = linyaps_box::utils::statfs(old_root);
    LINYAPS_BOX_DEBUG() << "Pivot root old root: " << dump_mount_flags(old_root_stat.f_flags);

    auto new_root_stat = linyaps_box::utils::statfs(new_root);
    LINYAPS_BOX_DEBUG() << "Pivot root new root: " << dump_mount_flags(new_root_stat.f_flags);

    auto ret = fchdir(new_root.get());
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "fchdir");
    }

    LINYAPS_BOX_DEBUG() << "Pivot root new root: "
                        << linyaps_box::utils::inspect_fd(new_root.get());
    ret = syscall(__NR_pivot_root, ".", ".");
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

void set_umask(const std::optional<mode_t> &mask)
{
    if (!mask) {
        LINYAPS_BOX_DEBUG() << "Skip set umask";
        return;
    }

    LINYAPS_BOX_DEBUG() << "Set umask: " << std::oct << mask.value();
    umask(mask.value());
}

unsigned long get_last_cap()
{
    const auto *file = "/proc/sys/kernel/cap_last_cap";
    std::ifstream ifs(file);
    if (!ifs) {
        throw std::runtime_error("Can't open " + std::string(file));
    }

    unsigned long last_cap{ 0 };
    ifs >> last_cap;
    return last_cap;
}

security_status get_runtime_security_status()
{
    // TODO: selinux/apparmor
    security_status status;

#ifdef LINYAPS_BOX_ENABLE_CAP
    status.cap = get_last_cap();
#endif

    return status;
}

void set_capabilities(const linyaps_box::container &container, int last_cap)
{
#ifdef LINYAPS_BOX_ENABLE_CAP
    LINYAPS_BOX_DEBUG() << "Set capabilities";
    const auto &capabilities = container.get_config().process.capabilities;

    if (last_cap <= 0) {
        throw std::runtime_error("kernel does not support capabilities");
    }

    const auto &bounding_set = capabilities.bounding;
    for (int cap = 0; cap < last_cap; ++cap) {
        if (std::find_if(bounding_set.cbegin(),
                         bounding_set.cend(),
                         [cap](int c) {
                             return c == cap;
                         })
            == bounding_set.cend()) {
            if (cap_drop_bound(cap) < 0) {
                throw std::system_error(errno, std::generic_category(), "cap_drop_bound");
            }
        }
    }

    std::unique_ptr<_cap_struct, decltype(&cap_free)> caps(cap_init(), cap_free);
    int ret{ -1 };
    const auto &effective_set = capabilities.effective;
    if (!effective_set.empty()) {
        ret = cap_set_flag(caps.get(),
                           CAP_EFFECTIVE,
                           capabilities.effective.size(),
                           capabilities.effective.data(),
                           CAP_SET);
        if (ret < 0) {
            throw std::system_error(errno,
                                    std::generic_category(),
                                    "failed to set effective capabilities");
        }
    }

    const auto &permitted_set = capabilities.permitted;
    if (!permitted_set.empty()) {
        ret = cap_set_flag(caps.get(),
                           CAP_PERMITTED,
                           capabilities.permitted.size(),
                           capabilities.permitted.data(),
                           CAP_SET);
        if (ret < 0) {
            throw std::system_error(errno,
                                    std::generic_category(),
                                    "failed to set permitted capabilities");
        }
    }

    const auto &inheritable_set = capabilities.inheritable;
    if (!inheritable_set.empty()) {
        ret = cap_set_flag(caps.get(),
                           CAP_INHERITABLE,
                           capabilities.inheritable.size(),
                           capabilities.inheritable.data(),
                           CAP_SET);
        if (ret < 0) {
            throw std::system_error(errno,
                                    std::generic_category(),
                                    "failed to set inheritable capabilities");
        }
    }

    // keep current capabilities, we need these caps on later
    ret = prctl(PR_SET_KEEPCAPS, 1);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "keep current capabilities");
    }

    const auto &process = container.get_config().process;
    ret = setresuid(process.user.uid, process.user.uid, process.user.uid);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "setresuid");
    }

    ret = setresgid(process.user.gid, process.user.gid, process.user.gid);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "setresgid");
    }

    ret = cap_set_proc(caps.get());
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "cap_set_proc");
    }

#ifdef PR_CAP_AMBIENT
    ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0L, 0L, 0L);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "cap_ambient_clear_all");
    }

    std::for_each(capabilities.ambient.cend(), capabilities.ambient.cend(), [](cap_value_t cap) {
        auto ret = prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0L, 0L);
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(), "cap_ambient_raise");
        }
    });
#endif

#endif

    if (container.get_config().process.no_new_privileges) {
        LINYAPS_BOX_DEBUG() << "Set no new privileges";
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
            throw std::system_error(errno, std::generic_category(), "prctl");
        }
    }
}

void start_container_hooks(const linyaps_box::container &container,
                           linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.start_container) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Execute start container hooks";

    for (const auto &hook : container.get_config().hooks.start_container.value()) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Start container hooks executed";

    socket << std::byte(sync_message::STARTCONTAINER_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void close_other_fds(std::set<uint> except_fds)
{
    LINYAPS_BOX_DEBUG() << "Close all fds excepts " << [&]() -> std::string {
        std::stringstream ss;
        for (auto fd : except_fds) {
            ss << fd << " ";
        }
        return ss.str();
    }();

    except_fds.insert(0);
    except_fds.insert(std::numeric_limits<uint>::max());

    for (auto fd = except_fds.begin(); std::next(fd) != except_fds.end(); ++fd) {
        auto low = *fd + 1;
        auto high = *std::next(fd) - 1;
        if (low >= high) {
            continue;
        }

        linyaps_box::utils::close_range(low, high, 0);
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

    auto rootfs = container.get_config().root.path;
    if (rootfs.is_relative()) {
        rootfs = std::filesystem::canonical(container.get_bundle() / rootfs);
    }

    initialize_container(container.get_config(), socket);
    auto [runtime_cap] = get_runtime_security_status(); // get runtime status before pivot root
    configure_mounts(container, rootfs);
    wait_prestart_hooks_result(container, socket);
    wait_create_runtime_result(container, socket);
    create_container_hooks(container, socket);
    // TODO: selinux label/apparmor profile
    do_pivot_root(rootfs);
    set_umask(container.get_config().process.user.umask);
    set_capabilities(container, runtime_cap);
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

[[nodiscard]] int generate_clone_flag(
        const std::optional<std::vector<linyaps_box::config::linux_t::namespace_t>> &namespaces)
{
    LINYAPS_BOX_DEBUG() << "Generate clone flags";

    int flag = SIGCHLD;
    LINYAPS_BOX_DEBUG() << "Add SIGCHLD, flag=0x" << std::hex << flag;
    if (!namespaces) {
        return flag;
    }

    uint32_t setted_namespaces = 0;
    for (const auto &ns : *namespaces) {
        switch (ns.type) {
        case linyaps_box::config::linux_t::namespace_t::IPC: {
            flag |= CLONE_NEWIPC;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWIPC, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::UTS: {
            flag |= CLONE_NEWUTS;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWUTS, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::MOUNT: {
            flag |= CLONE_NEWNS;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWNS, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::PID: {
            flag |= CLONE_NEWPID;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWPID, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::NET: {
            flag |= CLONE_NEWNET;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWNET, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::USER: {
            flag |= CLONE_NEWUSER;
            LINYAPS_BOX_DEBUG() << "Add CLONE_NEWUSER, flag=0x" << std::hex << flag;
        } break;
        case linyaps_box::config::linux_t::namespace_t::CGROUP: {
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

void set_rlimits(const linyaps_box::config::process_t::rlimits_t &rlimits)
{
    std::for_each(rlimits.begin(),
                  rlimits.end(),
                  [](const linyaps_box::config::process_t::rlimit_t &rlimit) {
                      struct rlimit rl{ rlimit.soft, rlimit.hard };
                      auto resource = linyaps_box::utils::str_to_rlimit(rlimit.type);
                      LINYAPS_BOX_DEBUG() << "Set rlimit " << rlimit.type
                                          << ": Soft=" << rlimit.soft << ", Hard=" << rlimit.hard;
                      if (setrlimit(resource, &rl) == -1) {
                          throw std::system_error(errno, std::generic_category(), "setrlimit");
                      }
                  });
}

std::tuple<int, linyaps_box::utils::file_descriptor> start_container_process(
        const linyaps_box::container &container, const linyaps_box::config::process_t &process)
{
    LINYAPS_BOX_DEBUG() << "All opened file describers before socketpair:\n"
                        << linyaps_box::utils::inspect_fds();
    auto sockets = linyaps_box::utils::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    LINYAPS_BOX_DEBUG() << "All opened file describers after socketpair:\n"
                        << linyaps_box::utils::inspect_fds();
    const auto &config = container.get_config();

    // config rlimits before we enter new user namespace
    if (const auto &rlimits = config.process.rlimits; rlimits) {
        set_rlimits(rlimits.value());
    }

    std::optional<std::vector<linyaps_box::config::linux_t::namespace_t>> namespaces;
    if (config.linux && config.linux->namespaces) {
        namespaces = config.linux->namespaces;
    }

    int clone_flag = runtime_ns::generate_clone_flag(namespaces);
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

[[nodiscard]] int execute_user_namespace_helper(const std::vector<std::string> &args)
{
    LINYAPS_BOX_DEBUG() << "Execute user_namespace helper:" << [&]() -> std::string {
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
        throw std::system_error(errno, std::generic_category(), "fork");
    }

    if (pid == 0) {
        std::vector<const char *> c_args;
        c_args.reserve(args.size());
        for (const auto &arg : args) {
            c_args.push_back(arg.c_str());
        }

        c_args.push_back(nullptr);
        auto ret = execvp(c_args[0], const_cast<char *const *>(c_args.data()));
        if (ret < 0) {
            exit(errno);
        }

        exit(0);
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

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        throw std::runtime_error("user_namespace helper exited which caused by signal "
                                 + std::to_string(WTERMSIG(status)));
    }

    throw std::runtime_error("user_namespace helper exited abnormally");
}

void set_deny_groups(const linyaps_box::container &container, const std::filesystem::path &filepath)
{
    auto data = linyaps_box::get_private_data(container);
    if (data.deny_setgroups) {
        throw std::runtime_error("denying setgroups");
    }

    auto file = linyaps_box::utils::open(filepath, O_CLOEXEC | O_CREAT | O_TRUNC | O_WRONLY);
    auto ret = ::write(file.get(), "deny", 4);
    if (ret < 0) {
        throw std::system_error{ errno, std::system_category(), "write setgroups" };
    }

    data.deny_setgroups = true;
}

void configure_gid_mapping(pid_t pid, const linyaps_box::container &container)
{
    LINYAPS_BOX_DEBUG() << "Configure GID mappings";

    const auto &config = container.get_config();
    const auto &gid_mappings = config.linux->gid_mappings;
    if (!gid_mappings) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
        return;
    }
    const auto &gid_mappings_v = gid_mappings.value();

    std::string content;
    const auto len = gid_mappings_v.size();
    auto self_process = std::filesystem::path{ "/proc" } / std::to_string(pid);
    const auto is_single_mapping = (gid_mappings_v.size() == 1 && gid_mappings_v[0].size == 1
                                    && gid_mappings_v[0].host_id == gid_mappings_v[0].container_id);
    if (is_single_mapping) {
        const auto &data = linyaps_box::get_private_data(container);
        if (!data.deny_setgroups) {
            set_deny_groups(container,
                            std::filesystem::path{ "/proc" } / std::to_string(pid) / "setgroups");
        }

        const auto &mapping = gid_mappings_v[0];
        content.append(std::to_string(mapping.host_id) + " ");
        content.append(std::to_string(mapping.container_id) + " ");
        content.append(std::to_string(mapping.size));

        auto file = linyaps_box::utils::open(self_process / "gid_map",
                                             O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC);
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
        throw std::system_error(ret, std::generic_category(), "newgidmap");
    }

    // maybe we have CAP_SETGID?
    content.clear();
    for (std::size_t i = 0; i < len; ++i) {
        const auto &mapping = gid_mappings_v[i];
        content.append(std::to_string(mapping.host_id) + " ");
        content.append(std::to_string(mapping.container_id) + " ");
        content.append(std::to_string(mapping.size));

        if (i != len - 1) {
            content.push_back('\n');
        }
    };

    auto file = linyaps_box::utils::open(self_process / "gid_map",
                                         O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC);
    ret = ::write(file.get(), content.data(), content.size());
    if (ret > 0) {
        return;
    }

    throw std::system_error{ errno,
                             std::generic_category(),
                             "write to " + file.current_path().string() };
}

void configure_uid_mapping(pid_t pid, const linyaps_box::container &container)
{
    LINYAPS_BOX_DEBUG() << "Configure UID mappings";

    const auto &config = container.get_config();
    const auto &uid_mappings = config.linux->uid_mappings;
    if (!uid_mappings) {
        LINYAPS_BOX_DEBUG() << "Nothing to do";
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

        content.append(std::to_string(mapping.host_id) + " ");
        content.append(std::to_string(mapping.container_id) + " ");
        content.append(std::to_string(mapping.size));

        auto file = linyaps_box::utils::open(self_process / "uid_map",
                                             O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC);
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
        throw std::system_error(ret, std::generic_category(), "newuidmap");
    }

    // try to write mapping directly, maybe we have CAP_SETUID?
    content.clear();
    for (std::size_t i = 0; i < len; ++i) {
        const auto &mapping = uid_mappings_v[i];
        content.append(std::to_string(mapping.host_id) + " ");
        content.append(std::to_string(mapping.container_id) + " ");
        content.append(std::to_string(mapping.size));

        if (i != len - 1) {
            content.push_back('\n');
        }
    };

    auto file = linyaps_box::utils::open(self_process / "uid_map",
                                         O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC);
    ret = ::write(file.get(), content.data(), content.size());
    if (ret > 0) {
        return;
    }

    throw std::system_error{ errno,
                             std::generic_category(),
                             "write to " + file.current_path().string() };
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

    const auto &linux = container.get_config().linux;
    if (linux) {
        const auto &namespaces = linux->namespaces;
        if (namespaces) {
            if (std::find_if(namespaces->cbegin(),
                             namespaces->cend(),
                             [](const linyaps_box::config::linux_t::namespace_t &ns) -> bool {
                                 return ns.type == linyaps_box::config::linux_t::namespace_t::USER;
                             })
                != namespaces->end()) {
                auto pid = container.status().PID;

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

    LINYAPS_BOX_DEBUG() << "Container namespaces configured";

    byte = std::byte(sync_message::NAMESPACE_CONFIGURED);
    socket << byte;

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void prestart_hooks(const linyaps_box::container &container,
                    linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.prestart) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Waiting request to execute prestart hooks";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message != sync_message::REQUEST_PRESTART_HOOKS) {
        throw unexpected_sync_message(sync_message::REQUEST_PRESTART_HOOKS, message);
    }

    LINYAPS_BOX_DEBUG() << "Execute prestart hooks";

    for (const auto &hook : container.get_config().hooks.prestart.value()) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Prestart hooks executed";

    socket << std::byte(sync_message::PRESTART_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void create_runtime_hooks(const linyaps_box::container &container,
                          linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.create_runtime) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Waiting request to execute create runtime hooks";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message != sync_message::REQUEST_CREATERUNTIME_HOOKS) {
        throw unexpected_sync_message(sync_message::REQUEST_CREATERUNTIME_HOOKS, message);
    }

    LINYAPS_BOX_DEBUG() << "Execute create runtime hooks";

    for (const auto &hook : container.get_config().hooks.create_runtime.value()) {
        execute_hook(hook);
    }

    LINYAPS_BOX_DEBUG() << "Create runtime hooks executed";

    socket << std::byte(sync_message::CREATERUNTIME_HOOKS_EXECUTED);

    LINYAPS_BOX_DEBUG() << "Sync message sent";
}

void wait_create_container_result(const linyaps_box::container &container,
                                  linyaps_box::utils::file_descriptor &socket)
{
    if (!container.get_config().hooks.create_container) {
        return;
    }

    LINYAPS_BOX_DEBUG()
            << "Waiting OCI runtime in container namespace send create container hooks result";

    std::byte byte{};
    socket >> byte;
    auto message = sync_message(byte);
    if (message == sync_message::CREATECONTAINER_HOOKS_EXECUTED) {
        LINYAPS_BOX_DEBUG() << "Create container hooks executed";
        return;
    }
    throw unexpected_sync_message(sync_message::CREATECONTAINER_HOOKS_EXECUTED, message);
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
    if (!container.get_config().hooks.poststart) {
        return;
    }

    for (const auto &hook : container.get_config().hooks.poststart.value()) {
        execute_hook(hook);
    }
}

void poststop_hooks(const linyaps_box::container &container) noexcept
{
    if (!container.get_config().hooks.poststop) {
        return;
    }

    for (const auto &hook : container.get_config().hooks.poststart.value()) {
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

linyaps_box::container::container(const status_directory &status_dir,
                                  const std::string &id,
                                  const std::filesystem::path &bundle,
                                  std::filesystem::path config,
                                  cgroup_manager_t manager)
    : container_ref(status_dir, id)
    , bundle(bundle)
    , data(new linyaps_box::container_data)
{
    if (config.is_relative()) {
        config = bundle / config;
    }

    std::ifstream ifs(config);
    if (!ifs) {
        throw std::runtime_error("Can't open config file " + config.string());
    }

    LINYAPS_BOX_DEBUG() << "load config from " << config;
    this->config = linyaps_box::config::parse(ifs);

    host_uid_ = ::geteuid();
    host_gid_ = ::getegid();

    // TODO: maybe find another way to get user name
#ifndef LINYAPS_BOX_STATIC_LINK
    auto *pw = getpwuid(host_uid_);
    if (pw == nullptr) {
        throw std::system_error(errno, std::generic_category(), "getpwuid");
    }
#endif
    {
        container_status_t status;
        status.oci_version = linyaps_box::config::oci_version;
        status.ID = id;
        status.PID = getpid();
        status.status = container_status_t::runtime_status::CREATING;
        status.bundle = bundle;
        status.created = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                std::chrono::system_clock::now().time_since_epoch())
                                                .count());
#ifndef LINYAPS_BOX_STATIC_LINK
        status.owner = pw->pw_name;
#endif
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

linyaps_box::container::~container() noexcept
{
    delete data;
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
int linyaps_box::container::run(const config::process_t &process) const
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
        runtime_ns::prestart_hooks(*this, socket);
        runtime_ns::create_runtime_hooks(*this, socket);
        runtime_ns::wait_create_container_result(*this, socket);
        runtime_ns::wait_socket_close(socket);

        {
            auto status = this->status();
            assert(status.status == container_status_t::runtime_status::CREATED);
            status.PID = child_pid;
            status.status = container_status_t::runtime_status::RUNNING;
            this->status_dir().write(status);
        }

        runtime_ns::poststart_hooks(*this);

        // TODO: support detach from the parent's process
        // Now we wait for the container process to exit
        container_process_exit_code = runtime_ns::wait_container_process(this->status().PID);

        runtime_ns::poststop_hooks(*this);
    } catch (const std::system_error &e) {
        LINYAPS_BOX_ERR() << "failed to run a container, caused by: " << e.what()
                          << ", code: " << e.code();
    } catch (const std::exception &e) {
        LINYAPS_BOX_ERR() << "failed to run a container, caused by: " << e.what();
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
