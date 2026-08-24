// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/platform.h"

#include "linyaps_box/log/macro.h"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstring>

#include <unistd.h>

namespace {
struct SignalItem
{
    std::string_view name;
    int value;
};
} // namespace

namespace linyaps_box::utils {
auto str_to_signal(std::string_view str) -> int
{
    if (str.rfind("SIG", 0) != std::string_view::npos) {
        str.remove_prefix(3);
    }

    // TODO: support real-time signal in the future?
    static constexpr std::array sig_list{
        SignalItem{ "ABRT", SIGABRT },     SignalItem{ "ALRM", SIGALRM },
        SignalItem{ "BUS", SIGBUS },       SignalItem{ "CHLD", SIGCHLD },
#ifdef SIGCLD
        SignalItem{ "CLD", SIGCLD }, // alias for CHLD
#endif
        SignalItem{ "CONT", SIGCONT },     SignalItem{ "FPE", SIGFPE },
        SignalItem{ "HUP", SIGHUP },       SignalItem{ "ILL", SIGILL },
        SignalItem{ "INT", SIGINT },       SignalItem{ "IO", SIGIO },
        SignalItem{ "IOT", SIGIOT },       SignalItem{ "KILL", SIGKILL },
        SignalItem{ "PIPE", SIGPIPE },     SignalItem{ "POLL", SIGPOLL },
        SignalItem{ "PROF", SIGPROF },     SignalItem{ "PWR", SIGPWR },
        SignalItem{ "QUIT", SIGQUIT },     SignalItem{ "SEGV", SIGSEGV },
        SignalItem{ "STOP", SIGSTOP },     SignalItem{ "SYS", SIGSYS },
        SignalItem{ "TERM", SIGTERM },     SignalItem{ "TRAP", SIGTRAP },
        SignalItem{ "TSTP", SIGTSTP },     SignalItem{ "TTIN", SIGTTIN },
        SignalItem{ "TTOU", SIGTTOU },     SignalItem{ "URG", SIGURG },
        SignalItem{ "USR1", SIGUSR1 },     SignalItem{ "USR2", SIGUSR2 },
        SignalItem{ "VTALRM", SIGVTALRM }, SignalItem{ "WINCH", SIGWINCH },
        SignalItem{ "XCPU", SIGXCPU },     SignalItem{ "XFSZ", SIGXFSZ }
    };

    constexpr auto sorted [[maybe_unused]] = []() noexcept {
        for (size_t i = 1; i < sig_list.size(); ++i) {
            if (sig_list[i - 1].name >= sig_list[i].name) {
                return false;
            }
        }
        return true;
    }();
    static_assert(sorted, "signal list must be sorted alphabetically");

    const auto *it = std::lower_bound(sig_list.cbegin(),
                                      sig_list.cend(),
                                      str,
                                      [](const SignalItem &item, std::string_view val) -> bool {
                                          return item.name < val;
                                      });
    if (it == sig_list.cend() || it->name != str) {
        throw std::invalid_argument("invalid signal name: " + std::string{ str });
    }

    return it->value;
}

auto get_page_size() noexcept -> std::size_t
{
    static const auto page_size = []() noexcept -> std::size_t {
        errno = 0;
        const auto sz = ::sysconf(_SC_PAGESIZE);

        if (sz == -1) {
            if (errno != 0) {
                LINYAPS_BOX_LOG_ERROR_ERRNO(errno, "Failed to get page size, defaulting to 4096");
            }

            return 4096;
        }

        return static_cast<std::size_t>(sz);
    }();

    return page_size;
}

auto is_invalid_env(std::string_view env) noexcept -> bool
{
    auto pos = env.find('=');
    if (pos == std::string_view::npos) {
        return true;
    }

    if (pos == 0) {
        return true;
    }

    if (env.find('\0') != std::string_view::npos) {
        return true;
    }

    return false;
}

} // namespace linyaps_box::utils
