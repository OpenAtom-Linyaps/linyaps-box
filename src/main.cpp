/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <unistd.h>

#include <iostream>

#include "util/logger.h"
#include "util/oci_runtime.h"
#include "container/container.h"
#include "container/container_option.h"

extern linglong::Runtime loadBundle(int argc, char **argv);

int main(int argc, char **argv)
{
    // TODO(iceyer): move loader to ll-loader?
    bool is_load_bundle = (argc == 4);

    linglong::Option opt;
    // TODO(iceyer): default in rootless
    if (geteuid() != 0) {
        opt.rootless = true;
    }

    try {
        linglong::Runtime r;
        std::string content;

        if (is_load_bundle) {
            r = loadBundle(argc, argv);
        } else {
            int readFD = atoi(argv[1]);

            if (readFD <= 0) {
                std::ifstream f(argv[1]);
                std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                content = str;
            } else {
                const size_t bufSize = 1024;
                char buf[bufSize] = {};
                size_t ret = 0;
                do {
                    ret = read(readFD, buf, bufSize);
                    if (ret > 0) {
                        content.append(buf, ret);
                    }
                } while (ret > 0);
            }

            if (readFD > 0) {
                r = linglong::fromString(content);
            } else {
                r = linglong::fromFile(argv[1]);
            }
        }

        if (linglong::util::fs::exists("/tmp/ll-debug")) {
            nlohmann::json j = r;
            std::ofstream debug(linglong::util::format("/tmp/ll-debug/%d.json", getpid()));
            debug << j.dump();
            debug.close();

            std::ofstream origin(linglong::util::format("/tmp/ll-debug/%d-orgin.json", getpid()));
            origin << content;
            origin.close();
        }

        linglong::Container c(r);
        return c.Start(opt);
    } catch (const std::exception &e) {
        logErr() << "failed: " << e.what();
        return -1;
    }
}
