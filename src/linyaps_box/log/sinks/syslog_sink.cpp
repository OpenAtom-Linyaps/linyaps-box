// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/syslog_sink.h"

#include "linyaps_box/utils/utils.h"

#include <cassert>
#include <utility>

#include <syslog.h>

namespace linyaps_box::log {

syslog_backend::syslog_backend(std::string ident) noexcept
    : ident_(std::move(ident))
    , opened(true)
{
    openlog();
}

syslog_backend::~syslog_backend() noexcept
{
    if (opened) {
        ::closelog();
    }
}

syslog_backend::syslog_backend(syslog_backend &&other) noexcept
    : ident_(std::move(other.ident_))
    , opened(std::exchange(other.opened, false))
{
    assert(opened);
    openlog();
}

auto syslog_backend::syslog(level lvl, std::string_view msg) const noexcept -> void
{
    if (UNLIKELY(!opened)) {
        return;
    }

    ::syslog(to_syslog_priority(lvl), "%.*s", static_cast<int>(msg.size()), msg.data());
}

auto syslog_backend::openlog() const noexcept -> void
{
    ::openlog(ident_.c_str(), 0, LOG_USER);
}

} // namespace linyaps_box::log
