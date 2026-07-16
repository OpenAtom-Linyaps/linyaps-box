// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sinks/journald_sink.h"

#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-journal.h>

namespace linyaps_box::log {

auto journald_backend::send(utils::span<const struct iovec> iov) noexcept -> void
{
    sd_journal_sendv(iov.data(), static_cast<int>(iov.size()));
}

} // namespace linyaps_box::log
