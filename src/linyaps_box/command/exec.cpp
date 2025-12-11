// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/exec.h"

#include "linyaps_box/impl/status_directory.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory.h"

void linyaps_box::command::exec(const struct exec_options &options)
{
    std::unique_ptr<status_directory> dir =
            std::make_unique<impl::status_directory>(options.global_.get().root);
    runtime_t runtime(std::move(dir));

    auto container_refs = runtime.containers();
    auto container = container_refs.find(options.ID);
    if (container == container_refs.end()) {
        throw std::runtime_error("container not found");
    }

    config::process_t proc;
    proc.cwd = options.cwd.value_or("/");
    proc.args = options.command;
    proc.terminal = isatty(STDIN_FILENO) == 1 && isatty(STDOUT_FILENO) == 1;
    proc.no_new_privileges = options.no_new_privs;
    proc.env = options.envs.value_or(std::vector<std::string>{});

#ifdef LINYAPS_BOX_ENABLE_CAP
    if (options.caps) {
        const auto &caps = options.caps.value();
        auto transform_cap = [&caps](std::vector<cap_value_t> &cap_set) {
            std::transform(
                    caps.cbegin(),
                    caps.cend(),
                    std::back_inserter(cap_set),
                    [](const std::string &cap) {
                        cap_value_t val{ 0 };
                        if (cap_from_name(cap.c_str(), &val) < 0) {
                            throw std::system_error(errno, std::system_category(), "cap_from_name");
                        }

                        return val;
                    });
        };

        transform_cap(proc.capabilities.effective);
        transform_cap(proc.capabilities.ambient);
        transform_cap(proc.capabilities.bounding);
        transform_cap(proc.capabilities.permitted);
    }
#endif

    // TODO: support exec fully

    container->second.exec(proc);
}
