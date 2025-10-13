// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <string> // IWYU pragma: keep

#if _GLIBCXX_RELEASE >= 15

#pragma GCC diagnostic push

#if defined(__clang__)
#pragma GCC diagnostic ignored "-W#warnings"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wcpp"
#endif

#endif

#include "gtest/gtest.h"

#if _GLIBCXX_RELEASE >= 15
#pragma GCC diagnostic pop
#endif

TEST(LINYAPS_BOX, Placeholder1)
{
    EXPECT_EQ(1, 1);
}
