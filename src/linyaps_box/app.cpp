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
            for (const char *c = argv[i]; *c != '\0'; ++c) {
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

    return std::visit(subCommand{ [](const command::list_options &options) {
                                     command::list(options);
                                     return 0;
                                 },
                                  [](const command::exec_options &options) {
                                      command::exec(options);
                                      return 0;
                                  },
                                  [](const command::kill_options &options) {
                                      command::kill(options);
                                      return 0;
                                  },
                                  [](const command::run_options &options) {
                                      return command::run(options);
                                  },
                                  [code = options.global.return_code](const std::monostate &) {
                                      return code;
                                  } },
                      options.subcommand_opt);
} catch (const std::exception &e) {
    LINYAPS_BOX_ERR() << "Error: " << e.what();
    return -1;
} catch (...) {
    LINYAPS_BOX_ERR() << "unknown error";
    return -1;
}

} // namespace linyaps_box
