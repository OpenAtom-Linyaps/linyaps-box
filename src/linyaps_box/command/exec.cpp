// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/exec.h"

#include "linyaps_box/impl/status_directory.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory.h"

void linyaps_box::command::exec(const std::filesystem::path &root,
                                const struct exec_options &options)
{
    std::unique_ptr<status_directory> dir = std::make_unique<impl::status_directory>(root);
    runtime_t runtime(std::move(dir));

    auto container_refs = runtime.containers();
    auto container = container_refs.find(options.ID);
    if (container == container_refs.end()) {
        throw std::runtime_error("container not found");
    }

    config::process_t proc;
    proc.cwd = options.cwd;
    proc.args = options.command;
    // TODO: impl

    container->second.exec(proc);
}
