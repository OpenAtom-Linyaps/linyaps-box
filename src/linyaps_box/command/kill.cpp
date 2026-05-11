// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/kill.h"

#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"
#include "linyaps_box/utils/platform.h"

#include <algorithm>

void linyaps_box::command::kill(const struct kill_options &options)
{
    auto signal{ options.signal };
    int sig{ -1 };
    while (true) {
        if (std::all_of(signal.cbegin(), signal.cend(), ::isdigit)) {
            sig = std::stoi(signal);
            break;
        }

        if (signal.rfind("SIG", 0) == std::string::npos) {
            signal.insert(0, "SIG");
        }

        sig = utils::str_to_signal(signal);
        break;
    }

    status_directory_manager mgr(options.global_.get().root);
    runtime_t runtime(std::move(mgr));
    const auto &containers = runtime.containers();
    for (const auto &[id, ref] : containers) {
        if (id != options.container) {
            continue;
        }

        ref.kill(sig);
        return;
    }

    throw std::runtime_error("container not found");
}
