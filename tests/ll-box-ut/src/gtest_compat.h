// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// Single include point for googletest/gmock plus the version-compatibility
// shims that let the suite build against googletest 1.8.1 and newer. Test files
// should include this instead of <gtest/gtest.h> / <gmock/gmock.h>.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// googletest < 1.10 named these without the "_SUITE" suffix.
#ifndef INSTANTIATE_TEST_SUITE_P
#  define INSTANTIATE_TEST_SUITE_P(prefix, suite, ...) \
      INSTANTIATE_TEST_CASE_P(prefix, suite, __VA_ARGS__)
#endif

// The modern TYPED_TEST_SUITE is variadic and callers pass a trailing empty
// argument to stay -Wpedantic-clean under C++17; the legacy macro takes exactly
// two arguments, so drop the trailing one.
#ifndef TYPED_TEST_SUITE
#  define TYPED_TEST_SUITE(suite, types, ...) TYPED_TEST_CASE(suite, types)
#endif

// googletest < 1.10 has no GTEST_SKIP.
#ifndef GTEST_SKIP
#  define GTEST_SKIP()                                                 \
      return GTEST_MESSAGE_("Skipped test due to runtime conditions.", \
                            ::testing::TestPartResult::kSuccess)
#endif

// Parameter structs that carry a `name` are printed by name on failure instead
// of as raw bytes. Define via LINYAPS_BOX_NAMED_PRINTER after the struct.
#define LINYAPS_BOX_NAMED_PRINTER(CaseType)                                   \
    [[maybe_unused]] inline void PrintTo(const CaseType &c, std::ostream *os) \
    {                                                                         \
        *os << c.name;                                                        \
    }
