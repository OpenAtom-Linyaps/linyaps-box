// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <string_view>

namespace linyaps_box::config {

struct rlimit
{
    enum class type : std::uint8_t {
        as,
        core,
        cpu,
        data,
        fsize,
        locks,
        memlock,
        msgqueue,
        nice,
        nofile,
        nproc,
        rss,
        rtprio,
        rttime,
        sigpending,
        stack,
    };

    type type_;
    uint64_t soft;
    uint64_t hard;
};

LINYAPS_REGISTER_ENUM_TABLE(rlimit::type,
                            16,
                            { rlimit::type::as, "RLIMIT_AS" },
                            { rlimit::type::core, "RLIMIT_CORE" },
                            { rlimit::type::cpu, "RLIMIT_CPU" },
                            { rlimit::type::data, "RLIMIT_DATA" },
                            { rlimit::type::fsize, "RLIMIT_FSIZE" },
                            { rlimit::type::locks, "RLIMIT_LOCKS" },
                            { rlimit::type::memlock, "RLIMIT_MEMLOCK" },
                            { rlimit::type::msgqueue, "RLIMIT_MSGQUEUE" },
                            { rlimit::type::nice, "RLIMIT_NICE" },
                            { rlimit::type::nofile, "RLIMIT_NOFILE" },
                            { rlimit::type::nproc, "RLIMIT_NPROC" },
                            { rlimit::type::rss, "RLIMIT_RSS" },
                            { rlimit::type::rtprio, "RLIMIT_RTPRIO" },
                            { rlimit::type::rttime, "RLIMIT_RTTIME" },
                            { rlimit::type::sigpending, "RLIMIT_SIGPENDING" },
                            { rlimit::type::stack, "RLIMIT_STACK" })

void from_json(const nlohmann::json &j, rlimit &v);

} // namespace linyaps_box::config
