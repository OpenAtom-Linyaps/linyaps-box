// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <sys/syslog.h>

#include <sstream>

#include <syslog.h>

namespace linyaps_box::utils {

bool force_log_to_stderr();
bool stderr_is_a_tty();
unsigned int get_current_log_level();
std::string get_pid_namespace(int pid = 0);
std::string get_current_commond();

template<unsigned int level>
class Logger : public std::stringstream
{
public:
    using std::stringstream::stringstream;
    ~Logger();
};

extern template class Logger<LOG_EMERG>;
extern template class Logger<LOG_ALERT>;
extern template class Logger<LOG_CRIT>;
extern template class Logger<LOG_ERR>;
extern template class Logger<LOG_WARNING>;
extern template class Logger<LOG_NOTICE>;
extern template class Logger<LOG_INFO>;
extern template class Logger<LOG_DEBUG>;
} // namespace linyaps_box::utils

#ifndef LINYAPS_BOX_LOG_ENABLE_CONTEXT_PIDNS
#define LINYAPS_BOX_LOG_ENABLE_CONTEXT_PIDNS 1
#endif

#ifndef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
#define LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION 1
#endif

#define LINYAPS_BOX_STRINGIZE_DETAIL(x) #x
#define LINYAPS_BOX_STRINGIZE(x) LINYAPS_BOX_STRINGIZE_DETAIL(x)

#if LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
#define LINYAPS_BOX_LOG_SOURCE_LOCATION \
    << "SOURCE=" __FILE__ ":" LINYAPS_BOX_STRINGIZE(__LINE__) << std::endl << __PRETTY_FUNCTION__
#else
#define LINYAPS_BOX_LOG_SOURCE_LOCATION
#endif

#define LINYAPS_BOX_LOG(level)                                                           \
    if (__builtin_expect(level <= ::linyaps_box::utils::get_current_log_level(), false)) \
    ::linyaps_box::utils::Logger<level>() LINYAPS_BOX_LOG_SOURCE_LOCATION << std::endl << std::endl

#ifndef LINYAPS_BOX_ACTIVE_LOG_LEVEL
#define LINYAPS_BOX_ACTIVE_LOG_LEVEL LOG_DEBUG
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_EMERG
#define LINYAPS_BOX_EMERG() LINYAPS_BOX_LOG(LOG_EMERG)
#else
#define LINYAPS_BOX_EMERG() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_ALERT
#define LINYAPS_BOX_ALERT() LINYAPS_BOX_LOG(LOG_ALERT)
#else
#define LINYAPS_BOX_ALERT() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_CRIT
#define LINYAPS_BOX_CRIT() LINYAPS_BOX_LOG(LOG_CRIT)
#else
#define LINYAPS_BOX_CRIT() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_ERR
#define LINYAPS_BOX_ERR() LINYAPS_BOX_LOG(LOG_ERR)
#else
#define LINYAPS_BOX_ERR() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_WARNING
#define LINYAPS_BOX_WARNING() LINYAPS_BOX_LOG(LOG_WARNING)
#else
#define LINYAPS_BOX_WARNING() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_NOTICE
#define LINYAPS_BOX_NOTICE() LINYAPS_BOX_LOG(LOG_NOTICE)
#else
#define LINYAPS_BOX_NOTICE() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_INFO
#define LINYAPS_BOX_INFO() LINYAPS_BOX_LOG(LOG_INFO)
#else
#define LINYAPS_BOX_INFO() if constexpr (false)
#endif

#if LINYAPS_BOX_ACTIVE_LOG_LEVEL >= LOG_DEBUG
#define LINYAPS_BOX_DEBUG() LINYAPS_BOX_LOG(LOG_DEBUG)
#else
#define LINYAPS_BOX_DEBUG() if constexpr (false)
#endif
