/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "container/seccomp.h"
#include "util/oci_runtime.h"

#include <seccomp.h>
#include <sys/klog.h>

#include <climits>

TEST(OCI, libseccomp)
{
    auto seccompArch = seccomp_arch_resolve_name("x86_64");
    EXPECT_EQ(seccompArch, SCMP_ARCH_X86_64);
}

TEST(OCI, Seccomp)
{
    const size_t bufSize = 1024;
    char buf[bufSize];

    auto filepath = "../../test/data/demo/config-seccomp.json";
    auto r = linglong::fromFile(filepath);

    EXPECT_EQ(ConfigSeccomp(r.linux.seccomp), 0);

    getcwd(buf, bufSize);
    EXPECT_EQ(errno, EPERM);

    r.linux.seccomp->defaultAction = "INVALID_ACTION";
    EXPECT_EQ(ConfigSeccomp(r.linux.seccomp), -1);
}

TEST(OCI, SeccompDefault)
{
    const size_t bufSize = 1024;
    char buf[bufSize];

    auto filepath = "../../test/data/demo/config-seccomp-default.json";
    auto r = linglong::fromFile(filepath);

    EXPECT_EQ(ConfigSeccomp(r.linux.seccomp), 0);

    klogctl(2, buf, bufSize);
    EXPECT_EQ(errno, EPERM);

    // reset seccomp
    filepath = "../../test/data/demo/config-seccomp.json";
    r = linglong::fromFile(filepath);

    EXPECT_EQ(ConfigSeccomp(r.linux.seccomp), 0);

    klogctl(2, buf, bufSize);
    EXPECT_EQ(errno, EPERM);
}
