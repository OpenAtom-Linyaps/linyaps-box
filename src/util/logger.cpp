/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "logger.h"
#include <sys/syslog.h>

namespace linglong {
namespace util {

std::string errnoString()
{
    return util::format("errno(%d): %s", errno, strerror(errno));
}

std::string retErrString(int ret)
{
    return util::format("ret(%d),errno(%d): %s", ret, errno, strerror(errno));
}

std::string getPidNsPid()
{
    char buf[30];
    memset(buf, 0, sizeof(buf));
    readlink("/proc/self/ns/pid", buf, sizeof(buf) - 1);
    std::string str = buf;
    return str.substr(5, str.length() - 6) + ":" + std::to_string(getpid()); // 6 = strlen("pid:[]")
}

static Logger::Level getLogLevelFromStr(std::string str)
{
    if (str == "kDebug") {
        return Logger::kDebug;
    } else if (str == "kInfo") {
        return Logger::kInfo;
    } else if (str == "Warning") {
        return Logger::kWarring;
    } else if (str == "kError") {
        return Logger::kError;
    } else if (str == "kFatal") {
        return Logger::kFatal;
    } else {
        return Logger::Level(Logger::kFatal + 1);
    }
}

static Logger::Level initLogLevel()
{
    openlog("ll-box", LOG_PID, LOG_USER);
    auto env = getenv("LINGLONG_LOG_LEVEL");
    return getLogLevelFromStr(env ? env : "");
}

Logger::Level Logger::LOGLEVEL = initLogLevel();

} // namespace util
} // namespace linglong
