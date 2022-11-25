/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "util/oci_runtime.h"

using namespace linglong;

TEST(OCI, Runtime)
{
    auto r = linglong::fromFile("../../test/data/demo/config-mini.json");

    EXPECT_EQ(r.version, "1.0.1");
    EXPECT_EQ(r.process.args[0], "/bin/bash");
    EXPECT_EQ(r.process.env[1], "TERM=xterm");

    EXPECT_EQ(r.mounts.has_value(), true);
    EXPECT_EQ(r.mounts->at(1).data.at(1), "strictatime");

    EXPECT_EQ(r.hooks, tl::nullopt);

    EXPECT_EQ(r.linux.namespaces.size(), 5);

    EXPECT_EQ(r.linux.uidMappings.size(), 2);
    EXPECT_EQ(r.linux.uidMappings.at(1).hostID, 1000);
    EXPECT_EQ(r.linux.uidMappings.at(1).containerID, 1000);
    EXPECT_EQ(r.linux.uidMappings.at(1).size, 1);

    EXPECT_EQ(r.linux.gidMappings.size(), 2);
    EXPECT_EQ(r.linux.gidMappings.at(0).hostID, 65534);
    EXPECT_EQ(r.linux.gidMappings.at(0).containerID, 0);
    EXPECT_EQ(r.linux.gidMappings.at(0).size, 1);

    EXPECT_EQ(r.linux.seccomp->defaultAction, "SCMP_ACT_ALLOW");
    EXPECT_EQ(r.linux.seccomp->architectures.size(), 2);
    EXPECT_EQ(r.linux.seccomp->architectures[0], "SCMP_ARCH_X86");
    EXPECT_EQ(r.linux.seccomp->architectures[1], "SCMP_ARCH_X32");
    EXPECT_EQ(r.linux.seccomp->syscalls.size(), 1);
    EXPECT_EQ(r.linux.seccomp->syscalls[0].names.size(), 2);
    EXPECT_EQ(r.linux.seccomp->syscalls[0].action, "SCMP_ACT_ERRNO");

    EXPECT_EQ(r.linux.resources.cpu.shares, 512);
}

TEST(OCI, Util)
{
    EXPECT_EQ(util::format("%d %d %d\n", 1, 1, 1), "1 1 1\n");
    EXPECT_EQ(util::format("%d %d %d\n", uint64_t(1), 1u, 1), "1 1 1\n");
}

TEST(OCI, JSON)
{
    Runtime r;

    r.hooks = tl::nullopt;

    nlohmann::json j(r);

    EXPECT_EQ(j.at("hooks").is_null(), true);
}
