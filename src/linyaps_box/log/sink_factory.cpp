// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/log/sink_factory.h"

#include "linyaps_box/log/sinks/file_sink.h"
#include "linyaps_box/log/sinks/stderr_sink.h"
#include "linyaps_box/log/sinks/syslog_sink.h"
#include "linyaps_box/os/fs.h"

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
#  include "linyaps_box/log/sinks/journald_sink.h"
#endif

#include <filesystem>

namespace linyaps_box::log {

auto make_sink(std::string_view log_dest, output_format fmt, bool cee_syslog)
  -> std::unique_ptr<sink>
{
    constexpr auto file_log_flag =
      os::sys::open_option{ os::sys::open_flag::cloexec | os::sys::open_flag::append
                              | os::sys::open_flag::create,
                            os::sys::access_mode::write_only };
    constexpr auto file_log_perm =
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;

    auto idx = log_dest.find(':');
    if (idx == std::string_view::npos) {
        if (log_dest == "stderr") {
            return std::make_unique<stderr_sink>(stderr_spec{ }, fmt);
        }

        return std::make_unique<file_sink>(
          file_spec{
            linyaps_box::os::throw_if_error(linyaps_box::os::open(std::filesystem::path{ log_dest },
                                                                  file_log_flag,
                                                                  file_log_perm)) },
          fmt);
    }

    auto scheme = log_dest.substr(0, idx);
    auto content = log_dest.substr(idx + 1);

    if (scheme == "file") {
        return std::make_unique<file_sink>(file_spec{ linyaps_box::os::throw_if_error(
                                             linyaps_box::os::open(std::filesystem::path{ content },
                                                                   file_log_flag,
                                                                   file_log_perm)) },
                                           fmt);
    }

    if (scheme == "syslog") {
        return std::make_unique<syslog_sink>(syslog_spec{ std::string{ content }, cee_syslog },
                                             fmt);
    }

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
    if (scheme == "journald") {
        return std::make_unique<journald_sink>(journald_spec{ std::string{ content } }, fmt);
    }
#endif

    throw std::runtime_error(fmt::format("unknown log scheme: {}", scheme));
}

} // namespace linyaps_box::log
