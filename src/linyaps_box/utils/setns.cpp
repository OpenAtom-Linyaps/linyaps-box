// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/setns.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/enum_formatter.h" // IWYU pragma: keep
#include "linyaps_box/utils/utils.h"

#include <fmt/format.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

namespace {

auto to_proc_ns_string(linyaps_box::config::ns::type type) noexcept -> std::string_view
{
    switch (type) {
    case linyaps_box::config::ns::type::ipc:
        return "ipc";
    case linyaps_box::config::ns::type::uts:
        return "uts";
    case linyaps_box::config::ns::type::mount:
        return "mnt";
    case linyaps_box::config::ns::type::pid:
        return "pid";
    case linyaps_box::config::ns::type::net:
        return "net";
    case linyaps_box::config::ns::type::user:
        return "user";
    case linyaps_box::config::ns::type::cgroup:
        return "cgroup";
    case linyaps_box::config::ns::type::time:
        return "time";
    }

    __builtin_unreachable();
}

} // namespace

namespace linyaps_box::utils {

using namespace linyaps_box::config;

namespace {

// Joins namespace descriptors in USER-namespace-first order; a setns that the
// kernel rejects with EINVAL (namespace type unsupported here) is downgraded
// to a warning.  Shared by the create-time (join_namespaces_with_path) and
// exec-time (join_container_namespaces) paths.
void join_namespaces_in_order(std::vector<std::pair<ns::type, file_descriptor>> &ns_fds)
{
    for (auto &[type, fd] : ns_fds) {
        if (type != ns::type::user) {
            continue;
        }

        join_namespace(fd, type);
        break;
    }

    for (auto &[type, fd] : ns_fds) {
        if (type == ns::type::user) {
            continue;
        }

        try {
            join_namespace(fd, type);
        } catch (const std::system_error &e) {
            if (e.code().value() == EINVAL) {
                LINYAPS_BOX_LOG_WARN("setns for {} not supported", type);
                continue;
            }

            throw;
        }
    }
}

} // namespace

auto open_namespace_fd(pid_t target_pid, ns::type ns_type) -> file_descriptor
{
    auto path = std::filesystem::path{ "/proc" } / std::to_string(target_pid) / "ns"
      / to_proc_ns_string(ns_type);
    return os::throw_if_error(
      os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));
}

void setns(const file_descriptor &ns_fd, ns::type ns_type)
{
    // nstype is 0 for /proc/PID/ns/* file descriptors (kernel detects type from fd).
    // When PIDFD support is added, nstype will be derived from ns_type.
    std::ignore = ns_type;
    if (UNLIKELY(::setns(ns_fd.get(), 0) < 0)) {
        throw std::system_error(errno, std::system_category(), "setns");
    }
}

void join_namespace(const file_descriptor &ns_fd, ns::type ns_type)
{
    setns(ns_fd, ns_type);
}

void join_container_namespaces(pid_t target_pid, const linux &linux_config)
{
    if (!linux_config.namespaces) {
        return;
    }

    std::vector<std::pair<ns::type, file_descriptor>> ns_fds;
    ns_fds.reserve(linux_config.namespaces->size());
    for (const auto &ns : *linux_config.namespaces) {
        ns_fds.emplace_back(ns.type_, open_namespace_fd(target_pid, ns.type_));
    }

    join_namespaces_in_order(ns_fds);
}

auto verify_namespace_fd(const file_descriptor &fd, ns::type type) -> void
{
    // A namespace fd resolves to a magic symlink "type:[id]" (e.g.
    // "pid:[4026531836]"); match the type prefix.
    const auto link = std::filesystem::read_symlink(std::filesystem::path{ "/proc/self/fd" }
                                                    / std::to_string(fd.get()));
    const auto target = link.string();

    const auto colon_pos = target.find(':');
    if (UNLIKELY(colon_pos == std::string::npos)) {
        throw std::runtime_error(
          fmt::format("namespace fd /proc/self/fd/{} (resolves to '{}') is not a namespace file",
                      fd.get(),
                      target));
    }

    const auto type_from_fd = target.substr(0, colon_pos);
    const auto expected = to_proc_ns_string(type);
    if (UNLIKELY(type_from_fd != expected)) {
        throw std::runtime_error(
          fmt::format("namespace fd /proc/self/fd/{} resolves to '{}' namespace, not '{}'",
                      fd.get(),
                      type_from_fd,
                      expected));
    }
}

auto join_namespaces_with_path(const std::vector<ns> &namespaces) -> void
{
    std::vector<std::pair<ns::type, file_descriptor>> ns_fds;
    ns_fds.reserve(namespaces.size());

    for (const auto &ns : namespaces) {
        if (!ns.path) {
            continue;
        }

        auto fd = os::throw_if_error(
          os::open(*ns.path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));

        // Verify the namespace TYPE on the very descriptor that will be passed
        // to setns — check and use are pinned to the same fd, so there is no
        // time-of-check/time-of-use gap.
        verify_namespace_fd(fd, ns.type_);

        ns_fds.emplace_back(ns.type_, std::move(fd));
    }

    join_namespaces_in_order(ns_fds);
}

} // namespace linyaps_box::utils
