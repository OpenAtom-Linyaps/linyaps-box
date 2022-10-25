/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_LOGGER_H_
#define LINGLONG_BOX_SRC_UTIL_LOGGER_H_

#include "util.h"

#include <unistd.h>
#include <sys/syslog.h>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <utility>

namespace linglong {
namespace util {
std::string errnoString();
std::string retErrString(int);
std::string getPidNsPid();

class Logger
{
public:
    enum Level {
        kDebug,
        kInfo,
        kWarring,
        kError,
        kFatal,
    };

    explicit Logger(Level level, const char *fn, int line)
        : level(level)
        , function(fn)
        , line(line) {};

    ~Logger()
    {
        std::string prefix;
        auto pidNs = getPidNsPid();
        int syslogLevel = LOG_DEBUG;
        switch (level) {
        case kDebug:
            syslogLevel = LOG_DEBUG;
        case kInfo:
            syslogLevel = LOG_INFO;
        case kWarring:
            syslogLevel = LOG_WARNING;
        case kError:
            syslogLevel = LOG_ERR;
        case kFatal:
            syslogLevel = LOG_ERR;
        }
        syslog(syslogLevel, "%s|%s:%d %s", pidNs.c_str(), function, line, ss.str().c_str());
        if (level < LOGLEVEL) {
            return;
        }
        switch (level) {
        case kDebug:
            prefix = "[DBG |";
            std::cout << prefix << " " << pidNs << " | " << function << ":" << line << " ] " << ss.str() << std::endl;
            break;
        case kInfo:
            prefix = "[IFO |";
            std::cout << "\033[1;96m";
            std::cout << prefix << " " << pidNs << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case kWarring:
            prefix = "[WAN |";
            std::cout << "\033[1;93m";
            std::cout << prefix << " " << pidNs << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case kError:
            prefix = "[ERR |";
            std::cout << "\033[1;31m";
            std::cout << prefix << " " << pidNs << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case kFatal:
            prefix = "[FAL |";
            std::cout << "\033[1;91m";
            std::cout << prefix << " " << pidNs << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            abort();
        }
    }

    template<class T>
    Logger &operator<<(const T &x)
    {
        ss << x << " ";
        return *this;
    }

private:
    static Level LOGLEVEL;
    Level level = kDebug;
    const char *function;
    int line;
    std::ostringstream ss;
};
} // namespace util
} // namespace linglong

#define logDbg() (linglong::util::Logger(linglong::util::Logger::kDebug, __FUNCTION__, __LINE__))
#define logWan() (linglong::util::Logger(linglong::util::Logger::kWarring, __FUNCTION__, __LINE__))
#define logInf() (linglong::util::Logger(linglong::util::Logger::kInfo, __FUNCTION__, __LINE__))
#define logErr() (linglong::util::Logger(linglong::util::Logger::kError, __FUNCTION__, __LINE__))
#define logFal() (linglong::util::Logger(linglong::util::Logger::kFatal, __FUNCTION__, __LINE__))

#endif /* LINGLONG_BOX_SRC_UTIL_LOGGER_H_ */
