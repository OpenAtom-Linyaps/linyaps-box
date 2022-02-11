#include "logger.h"

namespace linglong {
namespace util {

std::string errnoString()
{
    return util::format("errno(%d): %s", errno, strerror(errno));
}

std::string RetErrString(int ret)
{
    return util::format("ret(%d),errno(%d): %s", ret, errno, strerror(errno));
}

static Logger::Level getLogLevelFromStr(std::string str)
{
    if (str == "Debug") {
        return Logger::Debug;
    } else if (str == "Info") {
        return Logger::Info;
    } else if (str == "Warning") {
        return Logger::Warring;
    } else if (str == "Error") {
        return Logger::Error;
    } else if (str == "Fatal") {
        return Logger::Fatal;
    } else {
        return Logger::Info;
    }
}

static Logger::Level initLogLevel()
{
    auto env = getenv("LINGLONG_LOG_LEVEL");
    return getLogLevelFromStr(env ? env : "");
}

Logger::Level Logger::LOGLEVEL = initLogLevel();

} // namespace util
} // namespace linglong
