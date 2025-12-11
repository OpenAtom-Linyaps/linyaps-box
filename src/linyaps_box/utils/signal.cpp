// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/signal.h"

#include <sys/signalfd.h>

#include <system_error>

namespace linyaps_box::utils {

auto sigfillset(sigset_t &set) -> void
{
    auto ret = ::sigfillset(&set);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "sigfillset");
    }
}

auto sigismember(const sigset_t &set, int signo) -> bool
{
    auto ret = ::sigismember(&set, signo);
    if (ret < 0) {
        const std::string msg{ "failed to check signal " + std::to_string(signo) };
        throw std::system_error(errno, std::system_category(), msg);
    }

    return ret == 1;
}

auto sigprocmask(int how, const sigset_t &new_set, sigset_t *old_set) -> void
{
    auto ret = ::sigprocmask(how, &new_set, old_set);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "sigprocmask");
    }
}

auto sigaction(int sig, const struct sigaction &new_act, struct sigaction *old_act) -> void
{
    auto ret = ::sigaction(sig, &new_act, old_act);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "sigaction");
    }
}

auto reset_signals(const sigset_t &set) -> void
{
    struct sigaction act{};
    act.sa_handler = SIG_DFL;

    for (int sig = 1; sig <= SIGRTMAX; ++sig) {
        if (!sigismember(set, sig)) {
            continue;
        }

        if (sig == SIGKILL || sig == SIGSTOP) {
            continue;
        }

        auto ret = sigaction(sig, &act, nullptr);
        if (ret < 0) {
            const std::string msg{ "failed to reset signal " + std::to_string(sig) };
            throw std::system_error(errno, std::system_category(), msg);
        }
    }
}

auto create_signalfd(sigset_t &set, bool nonblock) -> file_descriptor
{
    unsigned flags = SFD_CLOEXEC;
    if (nonblock) {
        flags |= SFD_NONBLOCK;
    }

    auto ret = ::signalfd(-1, &set, static_cast<int>(flags));
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "signalfd");
    }

    return file_descriptor(ret);
}

} // namespace linyaps_box::utils
