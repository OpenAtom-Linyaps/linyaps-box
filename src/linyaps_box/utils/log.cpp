// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/log.h"

#include <linux/limits.h>

#include <array>
#include <chrono>
#include <iostream>

#include <sys/time.h>
#include <unistd.h>

#ifndef LINYAPS_BOX_LOG_DEFAULT_LEVEL
#define LINYAPS_BOX_LOG_DEFAULT_LEVEL LOG_DEBUG
#endif

namespace linyaps_box::utils {

template<unsigned int level>
Logger<level>::~Logger() noexcept
{
    if (level > get_current_log_level()) {
        return;
    }

    ss.flush();
    auto str = ss.str();

    syslog(level, "%s", str.c_str());

    if (!stderr_is_a_tty() && !force_log_to_stderr()) {
        return;
    }

    if constexpr (level <= LOG_ERR) {
        std::cerr << "\033[31m\033[1m";
    } else if constexpr (level <= LOG_WARNING) {
        std::cerr << "\033[33m\033[1m";
    } else if constexpr (level <= LOG_INFO) {
        std::cerr << "\033[34m";
    } else {
        std::cerr << "\033[0m";
    }

    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    std::cerr << "TIME=" << now.count() << " "
#if LINYAPS_BOX_LOG_ENABLE_CONTEXT_PIDNS
              << "PIDNS=" << get_pid_namespace() << " "
#endif
              << str << "\033[0m" << '\n'
#if LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
              << '\n'
#endif
            ;
    std::cerr.flush();
}

template class Logger<LOG_EMERG>;
template class Logger<LOG_ALERT>;
template class Logger<LOG_CRIT>;
template class Logger<LOG_ERR>;
template class Logger<LOG_WARNING>;
template class Logger<LOG_NOTICE>;
template class Logger<LOG_INFO>;
template class Logger<LOG_DEBUG>;

auto force_log_to_stderr() -> bool
{
    static const auto *result = getenv("LINYAPS_BOX_LOG_FORCE_STDERR");
    return result != nullptr;
}

auto stderr_is_a_tty() -> bool
{
    static const bool result = isatty(fileno(stderr)) != 0;
    return result;
}

namespace {
auto get_current_log_level_from_env() -> unsigned int
{
    auto *env = getenv("LINYAPS_BOX_LOG_LEVEL");
    if (env == nullptr) {
        return LINYAPS_BOX_LOG_DEFAULT_LEVEL;
    }

    auto ret = std::stoi(env);
    if (ret < 0) {
        return LOG_ALERT;
    }

    auto level = static_cast<unsigned int>(ret);
    if (level > LOG_DEBUG) {
        return LOG_DEBUG;
    }

    return level;
}
} // namespace

auto get_current_log_level() -> unsigned int
{
    static const unsigned int level = get_current_log_level_from_env();
    return level;
}

auto get_pid_namespace(int pid) -> std::string
{
    const auto &pidns_path = "/proc/" + ((pid != 0) ? std::to_string(pid) : "self") + "/ns/pid";

    std::array<char, PATH_MAX + 1> buf{};
    auto length = ::readlink(pidns_path.c_str(), buf.data(), PATH_MAX);
    if (length < 0) {
        return "not available";
    }

    const std::string_view result(buf.data(), static_cast<size_t>(length));
    constexpr std::string_view prefix = "pid:[";
    constexpr char suffix = ']';
    constexpr auto prefix_len = prefix.size();
    constexpr auto total_wrapper_len = prefix_len + 1; // "pid:[" + "]"

    if (result.size() < total_wrapper_len) {
        return "invalid format";
    }

    if (result.rfind(prefix, 0) != 0) {
        return "invalid format";
    }

    if (result.back() != suffix) {
        return "invalid format";
    }

    return std::string{ result.substr(prefix_len, result.size() - total_wrapper_len) };
}

} // namespace linyaps_box::utils
