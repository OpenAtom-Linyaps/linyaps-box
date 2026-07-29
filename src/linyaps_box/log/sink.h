// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/log/sinks/file_sink.h"

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
#  include "linyaps_box/log/sinks/journald_sink.h"
#endif

#include "linyaps_box/log/sinks/stderr_sink.h"
#include "linyaps_box/log/sinks/sync_socket_sink.h"
#include "linyaps_box/log/sinks/syslog_sink.h"

namespace linyaps_box::log {

using sink_variant = std::variant<std::monostate,
                                  stderr_sink,
                                  file_sink,
                                  syslog_sink,
                                  sync_socket_sink
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
                                  ,
                                  journald_sink
#endif
                                  >;

} // namespace linyaps_box::log
