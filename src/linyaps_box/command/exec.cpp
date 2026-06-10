// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/exec.h"

#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"

#ifdef LINYAPS_BOX_ENABLE_CAP
#  include <sys/capability.h>
#endif

auto linyaps_box::command::exec(const struct exec_options &options) -> int
{
    status_directory_manager mgr(options.global_.get().root);
    runtime_t runtime(std::move(mgr));

    auto container_refs = runtime.containers();
    auto container = container_refs.find(options.ID);
    if (container == container_refs.end()) {
        throw std::runtime_error("container not found");
    }

    exec_container_option option;
    option.proc.cwd = options.cwd.value_or("/");
    option.proc.args = options.command;
    option.proc.terminal = options.tty;
    option.proc.no_new_privileges = options.no_new_privs;
    option.proc.env = options.envs;
    option.preserve_fds = options.preserve_fds;

    if (option.proc.terminal && options.console_socket) {
        option.console_socket = unix_socket::connect(options.console_socket.value());
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (options.caps) {
        const auto &caps = options.caps.value();
        if (!option.proc.capabilities) {
            option.proc.capabilities.emplace();
        }

        std::vector<cap_value_t> parsed;
        parsed.reserve(caps.size());
        for (const auto &name : caps) {
            cap_value_t val{ };
            if (cap_from_name(name.c_str(), &val) < 0) {
                throw std::system_error(errno, std::system_category(), "cap_from_name");
            }

            parsed.push_back(val);
        }

        option.proc.capabilities->effective = parsed;
        option.proc.capabilities->ambient = parsed;
        option.proc.capabilities->bounding = parsed;
        option.proc.capabilities->permitted = parsed;
    }
#endif

    return container->second.exec(std::move(option));
}
