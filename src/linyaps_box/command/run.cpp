// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/run.h"

#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"
#include "linyaps_box/utils/utils.h"

auto linyaps_box::command::run(const struct run_options &options) -> int
{
    status_directory_manager mgr(options.global_.get().root);
    runtime_t runtime(std::move(mgr));
    const create_container_options_t create_container_options{ options.global_.get().manager,
                                                               options.ID,
                                                               options.bundle,
                                                               options.config };
    auto container = runtime.create_container(create_container_options);

    run_container_options_t run_options;
    run_options.preserve_fds = options.preserve_fds;

    const auto &cfg = container.get_config();
    if (UNLIKELY(!cfg.process || !cfg.root)) {
        throw std::runtime_error("'process' and 'root' are required for run a container");
    }

    if (container.get_config().process->terminal && options.console_socket) {
        run_options.console_socket = unix_socket::connect(options.console_socket.value());
    }

    return container.run(std::move(run_options));
}
