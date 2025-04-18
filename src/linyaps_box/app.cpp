// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/app.h"

#include "linyaps_box/command/exec.h"
#include "linyaps_box/command/kill.h"
#include "linyaps_box/command/list.h"
#include "linyaps_box/command/run.h"
#include "linyaps_box/utils/log.h"

#include <iostream>
#include <stdexcept>

namespace {

template<typename... T>
struct subCommand : T...
{
    using T::operator()...;
};

template<typename... T>
subCommand(T...) -> subCommand<T...>;

} // namespace

namespace linyaps_box {

// The main function of the ll-box,
// it is the entry point.
// Command line arguments are parsed according to
// https://github.com/opencontainers/runtime-tools/blob/v0.9.0/docs/command-line-interface.md
// Extended commands and options should be compatible with crun.
int main(int argc, char **argv) noexcept
try {
    LINYAPS_BOX_DEBUG() << "linyaps box called with" << [=]() -> std::string {
        std::stringstream result;
        for (int i = 0; i < argc; ++i) {
            result << " \"";
            for (char *c = argv[i]; *c; ++c) {
                if (*c == '\\') {
                    result << "\\\\";
                } else if (*c == '"') {
                    result << "\\\"";
                } else {
                    result << *c;
                }
            }
            result << "\"";
        }
        return result.str();
    }();

    command::options options = command::parse(argc, argv);
    if (options.global.return_code != 0) {
        return options.global.return_code;
    }

    std::visit(subCommand{ [](const command::list_options &options) {
                              command::list(options);
                          },
                           [](const command::exec_options &options) {
                               command::exec(options);
                           },
                           [](const command::kill_options &options) {
                               command::kill(options);
                           },
                           [](const command::run_options &options) {
                               options.global.get().return_code = command::run(options);
                           },
                           [](const std::monostate &) {
                               throw std::runtime_error("unknown subcommand");
                           } },
               options.subcommand_opt);

    return options.global.return_code;
} catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return -1;
} catch (...) {
    std::cerr << "Error: unknown" << std::endl;
    return -1;
}

} // namespace linyaps_box
