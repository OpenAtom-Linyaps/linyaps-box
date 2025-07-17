// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/run.h"

#include "linyaps_box/impl/status_directory.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory.h"

int linyaps_box::command::run(const struct run_options &options)
{
    std::unique_ptr<status_directory> dir =
            std::make_unique<impl::status_directory>(options.global.get().root);
    runtime_t runtime(std::move(dir));
    runtime_t::create_container_options_t create_container_options;
    create_container_options.bundle = options.bundle;
    create_container_options.config = options.config;
    create_container_options.ID = options.ID;
    create_container_options.manager = options.global.get().manager;

    auto container = runtime.create_container(create_container_options);

    return container.run(container.get_config().process);
}
