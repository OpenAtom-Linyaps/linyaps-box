// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/log.h"

#include <linux/limits.h>

#include <cstring>
#include <iostream>

#include <sys/time.h>
#include <unistd.h>

#ifndef LINYAPS_BOX_LOG_DEFAULT_LEVEL
#define LINYAPS_BOX_LOG_DEFAULT_LEVEL LOG_DEBUG
#endif

namespace linyaps_box::utils {

template<unsigned int level>
Logger<level>::~Logger()
{
    if (level > get_current_log_level()) {
        return;
    }

    auto str = this->str();

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

    timeval tv;
    gettimeofday(&tv, nullptr);

    std::cerr << "TIME=" << tv.tv_sec << "." << tv.tv_usec << " "
#if LINYAPS_BOX_LOG_ENABLE_CONTEXT_PIDNS
              << "PIDNS=" << get_pid_namespace() << " "
#endif
              << str << "\033[0m" << std::endl
#if LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
              << std::endl
#endif
            ;
}

template class Logger<LOG_EMERG>;
template class Logger<LOG_ALERT>;
template class Logger<LOG_CRIT>;
template class Logger<LOG_ERR>;
template class Logger<LOG_WARNING>;
template class Logger<LOG_NOTICE>;
template class Logger<LOG_INFO>;
template class Logger<LOG_DEBUG>;

bool force_log_to_stderr()
{
    static auto result = getenv("LINYAPS_BOX_LOG_FORCE_STDERR");
    return result;
}

bool stderr_is_a_tty()
{
    static bool result = isatty(fileno(stderr));
    return result;
}

namespace {
static unsigned int get_current_log_level_from_env()
{
    auto env = getenv("LINYAPS_BOX_LOG_LEVEL");
    if (!env) {
        return LINYAPS_BOX_LOG_DEFAULT_LEVEL;
    }

    auto level = atoi(env);
    if (level < 0) {
        return LOG_ALERT;
    }

    if (level > LOG_DEBUG) {
        return LOG_DEBUG;
    }

    return level;
}
} // namespace

unsigned int get_current_log_level()
{
    static unsigned int level = get_current_log_level_from_env();
    return level;
}

std::string get_pid_namespace(int pid)
{
    std::string pidns_path = "/proc/" + (pid ? std::to_string(pid) : "self") + "/ns/pid";

    char buf[PATH_MAX + 1];
    auto length = readlink(pidns_path.c_str(), buf, PATH_MAX);
    if (length < 0) {
        return "not available";
    }
    buf[length] = '\0';

    std::string result = buf;

    if (result.rfind("pid:[", 0) != 0) {
        std::abort();
    }

    if (result.back() != ']') {
        std::abort();
    }

    return result.substr(sizeof("pid:[") - 1, result.size() - 6);
}

} // namespace linyaps_box::utils
