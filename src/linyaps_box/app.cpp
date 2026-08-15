// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/app.h"

#include "linyaps_box/command/exec.h"
#include "linyaps_box/command/kill.h"
#include "linyaps_box/command/list.h"
#include "linyaps_box/command/run.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/log/sink_factory.h"
#include "linyaps_box/utils/utils.h"

#include <iostream>

namespace linyaps_box {

namespace {

auto initialize_logger(const linyaps_box::command::global_options &opts) -> bool
try {
    auto &logger = log::global_logger::instance();
    logger.set_level(opts.log_level);

    auto fmt = opts.log_format;

    if (opts.log.empty()) {
        std::vector<std::unique_ptr<log::sink>> sinks;
        sinks.push_back(log::make_sink("stderr", fmt, false));
        logger.set_sinks(std::move(sinks));
        return true;
    }

    std::vector<std::unique_ptr<log::sink>> sinks;
    sinks.reserve(opts.log.size());
    for (const auto &spec_str : opts.log) {
        sinks.push_back(log::make_sink(spec_str, fmt, opts.cee_syslog));
    }

    logger.set_sinks(std::move(sinks));
    return true;
} catch (std::exception &e) {
    fmt::println(std::cerr, "failed to initialize logger: {}", e.what());
    return false;
} catch (...) {
    fmt::println(std::cerr, "failed to initialize logger: unknown exception");
    return false;
}

} // namespace

// The main function of the ll-box
// Command line arguments are parsed according to
// https://github.com/opencontainers/runtime-tools/blob/v0.9.0/docs/command-line-interface.md
auto main(int argc, char **argv) noexcept -> int
{
    auto result = command::parse(argc, argv);
    if (!result) {
        return EXIT_FAILURE;
    }

    auto &opts = *result;

    // --help / --version: no logger initialization, no side effects
    if (std::holds_alternative<std::monostate>(opts.subcommand_opt)) {
        return EXIT_SUCCESS;
    }

    if (!initialize_logger(opts.global)) {
        return EXIT_FAILURE;
    }

    try {
        return std::visit(utils::Overload{ [&opts](const command::list_options &list) -> int {
                                              return command::list(list, opts.global);
                                          },
                                           [&opts](command::exec_options &exec) -> int {
                                               return command::exec(std::move(exec), opts.global);
                                           },
                                           [&opts](const command::kill_options &kill) -> int {
                                               return command::kill(kill, opts.global);
                                           },
                                           [&opts](const command::run_options &run) -> int {
                                               return command::run(run, opts.global);
                                           },
                                           [](const std::monostate &) -> int {
                                               // just for exhausting variant
                                               return EXIT_SUCCESS;
                                           } },
                          opts.subcommand_opt);
    } catch (const std::exception &e) {
        LINYAPS_BOX_LOG_ERROR("Error: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        LINYAPS_BOX_LOG_ERROR("Unknown error");
        return EXIT_FAILURE;
    }
}

} // namespace linyaps_box
