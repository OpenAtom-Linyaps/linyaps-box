/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_UTIL_LOGGER_H_
#define LINGLONG_BOX_SRC_UTIL_LOGGER_H_

#include "util.h"

#include <unistd.h>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <utility>

namespace linglong {
namespace util {
class Logger
{
public:
    enum Level {
        Debug,
        Info,
        Warring,
        Error,
        Fatal,
    };

    explicit Logger(Level l, const char *fn, int line)
        : level(l)
        , function(fn)
        , line(line) {};

    ~Logger()
    {
        std::string prefix;
        const std::string buf = ss.str().substr(0, ss.str().size() - 1);
        switch (level) {
        case Debug:
            prefix = "[DBG |";
            std::cout << prefix << getpid() << " | " << function << ":" << line << " ] " << buf << std::endl;
            break;
        case Info:
            prefix = "[IFO |";
            std::cout << "\033[1;96m";
            std::cout << prefix << getpid() << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Warring:
            prefix = "[WAN |";
            std::cout << "\033[1;93m";
            std::cout << prefix << getpid() << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Error:
            prefix = "[ERR |";
            std::cout << "\033[1;31m";
            std::cout << prefix << getpid() << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            break;
        case Fatal:
            prefix = "[FAL |";
            std::cout << "\033[1;91m";
            std::cout << prefix << getpid() << " | " << function << ":" << line << " ] " << ss.str();
            std::cout << "\033[0m" << std::endl;
            // FIXME: need crash;
            break;
        }
    }

    template<class T>
    Logger &operator<<(const T &x)
    {
        ss << x << " ";
        return *this;
    }

private:
    Level level = Debug;
    const char *function;
    int line;
    std::ostringstream ss;
};
} // namespace util
} // namespace linglong

#define logDbg() (linglong::util::Logger(linglong::util::Logger::Debug, __FUNCTION__, __LINE__))
#define logWan() (linglong::util::Logger(linglong::util::Logger::Warring, __FUNCTION__, __LINE__))
#define logInf() (linglong::util::Logger(linglong::util::Logger::Info, __FUNCTION__, __LINE__))
#define logErr() (linglong::util::Logger(linglong::util::Logger::Error, __FUNCTION__, __LINE__))
#define logFal() (linglong::util::Logger(linglong::util::Logger::Fatal, __FUNCTION__, __LINE__))

namespace linglong {
namespace util {

inline std::string errnoString()
{
    return util::format("errno(%d): %s", errno, strerror(errno));
}

inline std::string RetErrString(int ret)
{
    return util::format("ret(%d),errno(%d): %s", ret, errno, strerror(errno));
}

} // namespace util
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_UTIL_LOGGER_H_ */
