// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/list.h"

#include "linyaps_box/impl/json_printer.h"
#include "linyaps_box/impl/table_printer.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"

void linyaps_box::command::list(const struct list_options &options)
{
    status_directory_manager mgr(options.global_.get().root);
    runtime_t runtime(std::move(mgr));

    std::unique_ptr<printer> printer;
    if (options.output_format == list_options::output_format_t::json) {
        printer = std::make_unique<impl::json_printer>();
    } else {
        printer = std::make_unique<impl::table_printer>();
    }

    auto containers = runtime.containers();
    std::vector<container_status_t> statuses;
    statuses.reserve(containers.size());
    for (const auto &[_, container] : containers) {
        statuses.emplace_back(container.status());
    }

    printer->print_statuses(statuses);
}
