// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/process.h"

#include "linyaps_box/os/details/ioctl_wrapper.h"
#include "linyaps_box/utils/utils.h"

#include <sys/prctl.h>
#include <sys/syscall.h>

#include <sys/stat.h>
#include <sys/wait.h>

#ifndef __NR_pidfd_open
#  define __NR_pidfd_open 434
#endif

#ifndef __NR_pidfd_send_signal
#  define __NR_pidfd_send_signal 424
#endif

namespace linyaps_box::os {

namespace {

auto prctl(int option,
           unsigned long arg2 = 0,
           unsigned long arg3 = 0,
           unsigned long arg4 = 0,
           unsigned long arg5 = 0) noexcept -> Result<int>
{
    auto ret = ::prctl(option, arg2, arg3, arg4, arg5);
    if (UNLIKELY(ret < 0)) {
        return unexpected{ make_error_code(errno) };
    }

    return ret;
}

} // namespace

auto pidfd_open(pid_t pid) noexcept -> Result<utils::file_descriptor>
{
    const auto fd = ::syscall(__NR_pidfd_open, pid, 0);
    if (LIKELY(fd >= 0)) {
        return utils::file_descriptor{ static_cast<int>(fd) };
    }

    return unexpected{ make_error_code(errno) };
}

auto pidfd_send_signal(utils::file_descriptor_ref pidfd, int signal) noexcept -> Result<void>
{
    auto ret = ::syscall(__NR_pidfd_send_signal, pidfd.get(), signal, nullptr, 0U);
    if (LIKELY(ret == 0)) {
        return { };
    }

    return unexpected{ make_error_code(errno) };
}

auto waitpid(pid_t pid, int &status, int options) noexcept -> Result<int>
{
    status = 0;
    while (true) {
        auto ret = ::waitpid(pid, &status, options);
        if (ret >= 0) {
            if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
                continue;
            }

            return ret;
        }

        if (errno == EINTR) {
            continue;
        }

        return unexpected{ make_error_code(errno) };
    }

    __builtin_unreachable();
}

auto get_exit_code(int status) noexcept -> Result<int>
{
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    return unexpected(std::make_error_code(std::errc::invalid_argument));
}

auto umask(std::filesystem::perms perm) noexcept -> Result<std::filesystem::perms>
{
    if (UNLIKELY(perm == std::filesystem::perms::unknown)) {
        return unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    return std::filesystem::perms{ ::umask(
      static_cast<mode_t>(perm & std::filesystem::perms::all)) };
}

auto set_child_subreaper(bool enabled) noexcept -> Result<void>
{
    return prctl(PR_SET_CHILD_SUBREAPER, enabled ? 1 : 0).transform([](int) { });
}

auto set_keep_capabilities(bool enabled) noexcept -> Result<void>
{
    return prctl(PR_SET_KEEPCAPS, enabled ? 1 : 0).transform([](int) { });
}

auto set_no_new_privileges(bool state) noexcept -> Result<void>
{
    return prctl(PR_SET_NO_NEW_PRIVS, state ? 1 : 0).transform([](int) { });
}

auto clear_ambient_capability_set() noexcept -> Result<void>
{
    return prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL).transform([](int) { });
}

auto add_ambient_capability(long cap) noexcept -> Result<void>
{
    return prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap).transform([](int) { });
}

auto set_control_terminal(utils::file_descriptor_ref fd) noexcept -> Result<void>
{
    return details::ioctl(fd, TIOCSCTTY, 0).transform([](int) { });
}

[[nodiscard]] auto kill_process(pid_t pid, int signal) noexcept -> Result<void>
{
    auto ret = ::kill(pid, signal);
    if (LIKELY(ret == 0)) {
        return { };
    }

    return unexpected{ make_error_code(errno) };
}

} // namespace linyaps_box::os
