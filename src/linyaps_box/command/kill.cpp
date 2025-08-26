// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/kill.h"

#include "linyaps_box/impl/status_directory.h"
#include "linyaps_box/runtime.h"
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

    auto status_dir = std::make_unique<impl::status_directory>(options.global_.get().root);
    if (!status_dir) {
        throw std::runtime_error("failed to create status directory");
    }

    runtime_t runtime(std::move(status_dir));
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
