// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/command/exec.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/runtime.h"
#include "linyaps_box/status_directory_manager.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <fstream>

auto linyaps_box::command::exec(exec_options options, const global_options &global) noexcept -> int
try {
    status_directory_manager mgr(global.root);
    runtime_t runtime(std::move(mgr));

    const auto &container_refs = runtime.containers();
    auto container = container_refs.find(options.ID);
    if (UNLIKELY(container == container_refs.cend())) {
        throw std::runtime_error("container not found");
    }

    exec_container_option option;
    option.preserve_fds = options.preserve_fds;

    if (options.process_file) {
        std::ifstream file(*options.process_file);
        if (UNLIKELY(!file)) {
            throw std::runtime_error("cannot open process file: " + options.process_file->string());
        }

        nlohmann::json j;
        file >> j;
        option.proc = j.get<oci_config::process_t>();
    }

    option.cwd = std::move(options.cwd);
    if (options.tty) {
        option.tty = true;
    }

    if (options.no_new_privs) {
        option.no_new_privs = true;
    }

    option.extra_envs = std::move(options.envs);
    if (options.user) {
        option.uid = options.user->uid;
        option.gid = options.user->gid;
    }
    option.command = std::move(options.command);

    auto needs_terminal = option.tty.value_or(false) || (option.proc && option.proc->terminal);
    if (needs_terminal && options.console_socket) {
        option.console_socket = infra::unix_socket::connect(*options.console_socket);
    }

#ifdef LINYAPS_BOX_ENABLE_CAP
    option.caps = std::move(options.caps);
#endif

    return container->second.exec(std::move(option));
} catch (const std::exception &e) {
    LINYAPS_BOX_LOG_ERROR("failed to exec: {}", e.what());
    return EXIT_FAILURE;
} catch (...) {
    LINYAPS_BOX_LOG_ERROR("failed to exec: unknown exception");
    return EXIT_FAILURE;
}
