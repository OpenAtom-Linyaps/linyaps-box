/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "util/logger.h"
#include "util/oci-runtime.h"
#include "container/container_option.h"

const std::string kLoadTemplate = R"KLT00(
{
	"hooks": null,
	"hostname": "linglong",
	"linux": {
		"gidMappings": [{
			"containerID": 0,
			"hostID": 1000,
			"size": 1
		}],
		"uidMappings": [{
			"containerID": 0,
			"hostID": 1000,
			"size": 1
		}],
		"namespaces": [{
				"type": "mount"
			},
			{
				"type": "pid"
			}
		]
	},
	"mounts": [{
		"destination": "/proc",
		"options": [],
		"source": "proc",
		"type": "proc"
	}],
	"ociVersion": "1.0.1",
	"process": {
		"args": [
			"/bin/bash"
		],
        "cwd":"/",
		"env": [
			"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
			"TERM=xterm",
			"_=/usr/bin/env",
			"PS1=️\\033[48;5;214;38;5;26m${debian_chroot:+($debian_chroot)}\\h ⚛ \\w\\033[0m",
			"LD_LIBRARY_PATH=/opt/apps/com.qq.weixin.deepin/files/libs",
			"QT_PLUGIN_PATH=/usr/lib/plugins",
			"QT_QPA_PLATFORM_PLUGIN_PATH=/usr/lib/plugins/platforms",
			"DISPLAY=:0",
			"LANG=zh_CN.UTF-8",
			"LANGUAGE=zh_CN",
			"XDG_SESSION_DESKTOP=deepin",
			"D_DISABLE_RT_SCREEN_SCALE=",
			"XMODIFIERS=@im=fcitx",
			"DESKTOP_SESSION=deepin",
			"DEEPIN_WINE_SCALE=2.00",
			"XDG_CURRENT_DESKTOP=Deepin",
			"XIM=fcitx",
			"XDG_SESSION_TYPE=x11=",
			"CLUTTER_IM_MODULE=fcitx",
			"QT4_IM_MODULE=",
			"GTK_IM_MODULE=fcitx"
		]
	},
	"root": {
		"path": "/run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd422/root",
		"readonly": false
	}
}
)KLT00";

using namespace linglong;

// start container from tmp mount path
linglong::Runtime loadBundle(int argc, char **argv)
{
    if (argc < 4) {
        throw std::runtime_error("invalid load args count");
    }

    auto r = linglong::fromString(kLoadTemplate);
    std::string id = argv[1];
    std::string bundleRoot = argv[2];
    std::string exec = argv[3];

    if (util::fs::exists(bundleRoot + "/opt")) {
        Mount m;
        m.source = bundleRoot + "/opt";
        m.destination = "/opt";
        r.mounts->push_back(m);
    } else {
        Mount m;
        m.source = "tmpfs";
        m.type = "tmpfs";
        m.options = {"nodev", "nosuid"};
        m.destination = "/opt";
        r.mounts->push_back(m);
    }

    if (util::fs::exists(bundleRoot + "files")) {
        Mount m;
        m.source = bundleRoot;
        m.destination = util::format("/opt/apps/%s", id.c_str());
        r.mounts->push_back(m);
    }

    // process runtime
    if (util::fs::exists(bundleRoot + "runtime")) {
        Mount m;
        m.source = bundleRoot + "runtime";
        m.destination = util::format("/runtime");
        r.mounts->push_back(m);
    }

    // process env
    r.process.env.push_back(util::format("XAUTHORITY=%s", getenv("XAUTHORITY")));
    r.process.env.push_back(util::format("XDG_RUNTIME_DIR=%s", getenv("XDG_RUNTIME_DIR")));
    r.process.env.push_back(util::format("DBUS_SESSION_BUS_ADDRESS=%s", getenv("DBUS_SESSION_BUS_ADDRESS")));
    r.process.env.push_back(util::format("HOME=%s", getenv("HOME")));

    r.process.cwd = getenv("HOME");

    r.process.args = util::str_vec {exec};
    //    r.process.args.push_back(exec);

    /// run/user/1000/linglong/375f5681145f4f4f9ffeb3a67aebd422/root
    char dirTemplate[] = "/run/user/1000/linglong/XXXXXX";
    char *dirName = mkdtemp(dirTemplate);
    if (dirName == NULL) {
        throw std::runtime_error("mkdtemp failed");
    }

    auto rootPath = util::fs::path(dirName) / "root";
    util::fs::create_directories(rootPath, 0755);

    r.root.path = rootPath.string();

    // FIXME: workaround id map
    r.linux.uidMappings[0].hostID = getuid();
    r.linux.gidMappings[0].hostID = getgid();

    return r;
}