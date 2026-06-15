// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/kill.h"

#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"

auto linyaps_box::command::kill(const kill_options &options, const global_options &global) -> int
{
    status_directory_manager mgr(global.root);
    runtime_t runtime(std::move(mgr));
    const auto &containers = runtime.containers();
    for (const auto &[id, ref] : containers) {
        if (id != options.container) {
            continue;
        }

        ref.kill(options.signal);
        return 0;
    }

    throw std::runtime_error("container not found");
}