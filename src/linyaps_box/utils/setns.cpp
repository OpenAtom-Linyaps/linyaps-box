// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/setns.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/utils.h"

#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sched.h>
#include <unistd.h>

namespace {

auto to_proc_ns_string(linyaps_box::oci_config::linux_t::namespace_t::type type) noexcept
  -> std::string_view
{
    switch (type) {
    case linyaps_box::oci_config::linux_t::namespace_t::type::IPC:
        return "ipc";
    case linyaps_box::oci_config::linux_t::namespace_t::type::UTS:
        return "uts";
    case linyaps_box::oci_config::linux_t::namespace_t::type::MOUNT:
        return "mnt";
    case linyaps_box::oci_config::linux_t::namespace_t::type::PID:
        return "pid";
    case linyaps_box::oci_config::linux_t::namespace_t::type::NET:
        return "net";
    case linyaps_box::oci_config::linux_t::namespace_t::type::USER:
        return "user";
    case linyaps_box::oci_config::linux_t::namespace_t::type::CGROUP:
        return "cgroup";
    case linyaps_box::oci_config::linux_t::namespace_t::type::TIME:
        return "time";
    }

    __builtin_unreachable();
}

} // namespace

namespace linyaps_box::utils {

auto open_namespace_fd(pid_t target_pid, oci_config::linux_t::namespace_t::type ns_type)
  -> file_descriptor
{
    auto path = std::filesystem::path{ "/proc" } / std::to_string(target_pid) / "ns"
      / to_proc_ns_string(ns_type);
    return os::throw_if_error(
      os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only }));
}

void setns(const file_descriptor &ns_fd, oci_config::linux_t::namespace_t::type ns_type)
{
    // nstype is 0 for /proc/PID/ns/* file descriptors (kernel detects type from fd).
    // When PIDFD support is added, nstype will be derived from ns_type.
    std::ignore = ns_type;
    if (UNLIKELY(::setns(ns_fd.get(), 0) < 0)) {
        throw std::system_error(errno, std::system_category(), "setns");
    }
}

void join_namespace(const file_descriptor &ns_fd, oci_config::linux_t::namespace_t::type ns_type)
{
    setns(ns_fd, ns_type);
}

void join_container_namespaces(pid_t target_pid, const oci_config::linux_t &linux_config)
{
    if (!linux_config.namespaces) {
        return;
    }

    std::vector<std::pair<oci_config::linux_t::namespace_t::type, file_descriptor>> ns_fds;
    ns_fds.reserve(linux_config.namespaces->size());
    for (const auto &ns : *linux_config.namespaces) {
        ns_fds.emplace_back(ns.type_, open_namespace_fd(target_pid, ns.type_));
    }

    for (auto &[type, fd] : ns_fds) {
        if (type != oci_config::linux_t::namespace_t::type::USER) {
            continue;
        }

        join_namespace(fd, type);
        break;
    }

    for (auto &[type, fd] : ns_fds) {
        if (type == oci_config::linux_t::namespace_t::type::USER) {
            continue;
        }

        try {
            join_namespace(fd, type);
        } catch (const std::system_error &e) {
            if (e.code().value() == EINVAL) {
                LINYAPS_BOX_LOG_WARN("setns for {} not supported", to_string_view(type));
                continue;
            }

            throw;
        }
    }
}

} // namespace linyaps_box::utils
