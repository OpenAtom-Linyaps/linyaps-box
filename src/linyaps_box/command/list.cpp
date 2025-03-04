// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/list.h"

#include "linyaps_box/impl/json_printer.h"
#include "linyaps_box/impl/status_directory.h"
#include "linyaps_box/impl/table_printer.h"
#include "linyaps_box/runtime.h"

#include <memory>

int linyaps_box::command::list(const struct list_options &options)
{
    auto status_dir = std::make_unique<impl::status_directory>(options.global.root);
    if (!status_dir) {
        throw std::runtime_error("failed to create status directory");
    }

    runtime_t runtime(std::move(status_dir));

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
    return 0;
}
