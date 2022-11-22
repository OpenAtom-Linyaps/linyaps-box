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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>

#include "util/logger.h"
#include "util/oci_runtime.h"
#include "container/container.h"
#include "container/container_option.h"
#include "util/message_reader.h"

extern linglong::Runtime loadBundle(int argc, char **argv);

int main(int argc, char **argv)
{
    // TODO(iceyer): move loader to ll-loader?
    bool is_load_bundle = (argc == 4);

    linglong::Option option;
    // TODO(iceyer): default in rootless
    if (geteuid() != 0) {
        option.rootless = true;
    }

    try {
        linglong::Runtime runtime;
        nlohmann::json json;
        std::unique_ptr<linglong::util::MessageReader> reader = nullptr;

        if (is_load_bundle) {
            runtime = loadBundle(argc, argv);
            linglong::to_json(json, runtime);
        } else {
            int socket = atoi(argv[1]);
            if (socket <= 0) {
                socket = open(argv[1], O_RDONLY | O_CLOEXEC);
            }

            reader.reset(new linglong::util::MessageReader(socket));

            json = reader->read();

            runtime = json.get<linglong::Runtime>();
        }

        if (linglong::util::fs::exists("/tmp/ll-debug")) {
            std::ofstream origin(linglong::util::format("/tmp/ll-debug/%d.json", getpid()));
            origin << json.dump(4);
            origin.close();
        }

        linglong::Container container(runtime, std::move(reader));
        return container.Start(option);
    } catch (const std::exception &e) {
        logErr() << "failed: " << e.what();
        return -1;
    }
}
