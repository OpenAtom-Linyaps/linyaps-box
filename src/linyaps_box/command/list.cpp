// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/list.h"

#include "linyaps_box/container_status.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"
#include "linyaps_box/utils/date.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

namespace linyaps_box::command {

auto list(const list_options &options, const global_options &global) -> int
{
    status_directory_manager mgr(global.root);
    runtime_t runtime(std::move(mgr));

    const auto &containers = runtime.containers();

    std::vector<container_status> statuses;
    statuses.reserve(containers.size());
    for (const auto &[_, container] : containers) {
        statuses.emplace_back(container.status());
    }

    if (options.output_format == list_options::output_format_t::json) {
        auto j = nlohmann::json::array();
        auto *ptr = j.get_ptr<nlohmann::json::array_t *>();
        ptr->reserve(statuses.size());

        for (auto &s : statuses) {
            auto rs = derive_status(s);
            j += to_oci_json(std::move(s), rs);
        }

        fmt::println("{}", j.dump(4));
        return 0;
    }

    int max_length = 4;
    for (const auto &s : statuses) {
        max_length = std::max(max_length, static_cast<int>(s.id.length()));
    }

    constexpr std::string_view format_style{ "{:<{}}{:<10}{:<9}{:<40}{:<31}{}" };
    fmt::println(format_style,
                 "NAME",
                 max_length,
                 "PID",
                 "STATUS",
                 "BUNDLE PATH",
                 "CREATED",
                 "OWNER");

    std::array<char, 30> time_buf{ };
    for (const auto &s : statuses) {
        auto rs = derive_status(s);
        auto len = utils::to_created_time(utils::span{ time_buf }, s.created);
        fmt::println(format_style,
                     s.id,
                     max_length,
                     s.pid,
                     to_string_view(rs),
                     s.bundle,
                     std::string_view{ time_buf.cbegin(), len },
                     s.owner);
    }

    return 0;
}

} // namespace linyaps_box::command
