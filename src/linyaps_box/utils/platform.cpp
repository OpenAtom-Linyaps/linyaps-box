// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/platform.h"

#include <csignal>
#include <stdexcept>
#include <unordered_map>

namespace linyaps_box::utils {
int str_to_signal(std::string_view str)
{
    // Only support standard signals for now,
    // TODO: support real-time signal in the future
    static const std::unordered_map<std::string_view, int> sigMap{
        { "SIGABRT", SIGABRT },   { "SIGALRM", SIGALRM }, { "SIGBUS", SIGBUS },
        { "SIGCHLD", SIGCHLD },   { "SIGCONT", SIGCONT }, { "SIGFPE", SIGFPE },
        { "SIGHUP", SIGHUP },     { "SIGILL", SIGILL },   { "SIGINT", SIGINT },
        { "SIGKILL", SIGKILL },   { "SIGPIPE", SIGPIPE }, { "SIGPOLL", SIGPOLL },
        { "SIGPROF", SIGPROF },   { "SIGPWR", SIGPWR },   { "SIGQUIT", SIGQUIT },
        { "SIGSEGV", SIGSEGV },   { "SIGSTOP", SIGSTOP }, { "SIGSYS", SIGSYS },
        { "SIGTERM", SIGTERM },   { "SIGTRAP", SIGTRAP }, { "SIGTSTP", SIGTSTP },
        { "SIGTTIN", SIGTTIN },   { "SIGTTOU", SIGTTOU }, { "SIGURG", SIGURG },
        { "SIGUSR1", SIGUSR1 },   { "SIGUSR2", SIGUSR2 }, { "SIGVTALRM", SIGVTALRM },
        { "SIGWINCH", SIGWINCH }, { "SIGXCPU", SIGXCPU }, { "SIGXFSZ", SIGXFSZ },
        { "SIGIO", SIGIO },       { "SIGIOT", SIGIOT },   { "SIGCLD", SIGCLD },
    };

    auto it = sigMap.find(str);
    if (it == sigMap.end()) {
        throw std::invalid_argument("invalid signal name: " + std::string{ str });
    }

    return it->second;
}
} // namespace linyaps_box::utils
