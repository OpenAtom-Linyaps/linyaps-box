// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include <sys/prctl.h>

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // prevent death test generates coredump
    prctl(PR_SET_DUMPABLE, 0);

    auto result = RUN_ALL_TESTS();

    return result;
}
