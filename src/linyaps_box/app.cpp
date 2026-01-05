// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/app.h"

#include "linyaps_box/command/exec.h"
#include "linyaps_box/command/kill.h"
#include "linyaps_box/command/list.h"
#include "linyaps_box/command/run.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/utils.h"
#include "utils/log.h"

namespace linyaps_box {

// The main function of the ll-box,
// it is the entry point.
// Command line arguments are parsed according to
// https://github.com/opencontainers/runtime-tools/blob/v0.9.0/docs/command-line-interface.md
// Extended commands and options should be compatible with crun.
auto main(int argc, char **argv) noexcept -> int
try {
    LINYAPS_BOX_DEBUG() << "linyaps box called with" << [=]() -> std::string {
        std::stringstream result;
        for (int i = 0; i < argc; ++i) {
            result << " \"";
            for (const auto ch : std::string_view(argv[i])) { // NOLINT
                if (ch == '\\') {
                    result << "\\\\";
                } else if (ch == '"') {
                    result << "\\\"";
                } else {
                    result << ch;
                }
            }
            result << "\"";
        }
        return result.str();
    }();

    auto opts = command::parse(argc, argv);
    if (opts.global.return_code != 0) {
        return opts.global.return_code;
    }

    return std::visit(utils::Overload{ [](const command::list_options &options) {
                                          command::list(options);
                                          return 0;
                                      },
                                       [](const command::exec_options &options) -> int {
                                           return command::exec(options);
                                       },
                                       [](const command::kill_options &options) {
                                           command::kill(options);
                                           return 0;
                                       },
                                       [](const command::run_options &options) {
                                           return command::run(options);
                                       },
                                       [](const std::monostate &) {
                                           return 0;
                                       } },
                      opts.subcommand_opt);
} catch (const std::exception &e) {
    LINYAPS_BOX_ERR() << "Error: " << e.what();
    return -1;
} catch (...) {
    LINYAPS_BOX_ERR() << "unknown error";
    return -1;
}

} // namespace linyaps_box
