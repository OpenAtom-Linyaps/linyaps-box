// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/utils.h"

#include <syslog.h>

namespace linyaps_box::log {

auto to_syslog_priority(level lvl) noexcept -> int
{
    switch (lvl) {
    case level::fatal:
        return LOG_CRIT;
    case level::error:
        return LOG_ERR;
    case level::warn:
        return LOG_WARNING;
    case level::info:
        return LOG_INFO;
    case level::debug:
        return LOG_DEBUG;
    default:
        __builtin_unreachable();
    }
}

auto level_name(level lvl) noexcept -> const char *
{
    switch (lvl) {
    case level::fatal:
        return "FATAL";
    case level::error:
        return "ERROR";
    case level::warn:
        return "WARN";
    case level::info:
        return "INFO";
    case level::debug:
        return "DEBUG";
    default:
        __builtin_unreachable();
    }
}

} // namespace linyaps_box::log
