// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/platform.h"

#include "linyaps_box/utils/log.h"

#include <climits> // IWYU pragma: keep
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

#include <sys/resource.h>
#include <unistd.h>

namespace linyaps_box::utils {
auto str_to_signal(std::string_view str) -> int
{
    // Only support standard signals for now,
    // TODO: support real-time signal in the future
    const std::unordered_map<std::string_view, int> sigMap{
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
        { "SIGIO", SIGIO },       { "SIGIOT", SIGIOT },
#ifdef SIGCLD
        { "SIGCLD", SIGCLD },
#endif
    };

    auto it = sigMap.find(str);
    if (it == sigMap.end()) {
        throw std::invalid_argument("invalid signal name: " + std::string{ str });
    }

    return it->second;
}

auto str_to_rlimit(std::string_view str) -> int
{
    const std::unordered_map<std::string_view, int> resources{
        { "RLIMIT_AS", RLIMIT_AS },
        { "RLIMIT_CORE", RLIMIT_CORE },
        { "RLIMIT_CPU", RLIMIT_CPU },
        { "RLIMIT_DATA", RLIMIT_DATA },
        { "RLIMIT_FSIZE", RLIMIT_FSIZE },
        { "RLIMIT_MEMLOCK", RLIMIT_MEMLOCK },
        { "RLIMIT_MSGQUEUE", RLIMIT_MSGQUEUE },
        { "RLIMIT_NICE", RLIMIT_NICE },
        { "RLIMIT_NOFILE", RLIMIT_NOFILE },
        { "RLIMIT_NPROC", RLIMIT_NPROC },
        { "RLIMIT_RSS", RLIMIT_RSS },
        { "RLIMIT_RTPRIO", RLIMIT_RTPRIO },
        { "RLIMIT_RTTIME", RLIMIT_RTTIME },
        { "RLIMIT_SIGPENDING", RLIMIT_SIGPENDING },
        { "RLIMIT_STACK", RLIMIT_STACK },
    };

    auto it = resources.find(str);
    if (it == resources.end()) {
        throw std::invalid_argument("invalid resource name: " + std::string{ str });
    }

    return it->second;
}

auto get_path_max(const std::filesystem::path &fs_dir) noexcept -> std::size_t
{
    errno = 0;
    auto max = pathconf(fs_dir.c_str(), _PC_PATH_MAX);
    if (max == -1) {
        if (errno != 0) {
            auto saved_errno = errno;
            LINYAPS_BOX_WARNING() << "Failed to get pathconf: " << ::strerror(saved_errno)
                                  << ", use default value";
            errno = 0;
        }
#ifdef PATH_MAX
        return PATH_MAX;
#else
        // Should we make the default value to be configured value?
        return 4096;
#endif
    }

    return static_cast<std::size_t>(max);
}

} // namespace linyaps_box::utils
