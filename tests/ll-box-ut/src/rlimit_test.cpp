// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linyaps_box/utils/rlimit.h"

#include <sys/resource.h>

namespace linyaps_box {
namespace {

using namespace linyaps_box;
using namespace linyaps_box::config; // NOLINT

TEST(Rlimit, MapsEveryTypeToKernelResource)
{
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::as), RLIMIT_AS);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::core), RLIMIT_CORE);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::cpu), RLIMIT_CPU);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::data), RLIMIT_DATA);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::fsize), RLIMIT_FSIZE);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::locks), RLIMIT_LOCKS);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::memlock), RLIMIT_MEMLOCK);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::msgqueue), RLIMIT_MSGQUEUE);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::nice), RLIMIT_NICE);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::nofile), RLIMIT_NOFILE);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::nproc), RLIMIT_NPROC);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::rss), RLIMIT_RSS);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::rtprio), RLIMIT_RTPRIO);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::rttime), RLIMIT_RTTIME);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::sigpending), RLIMIT_SIGPENDING);
    EXPECT_EQ(utils::to_rlimit_resource(rlimit::type::stack), RLIMIT_STACK);
}

} // namespace
} // namespace linyaps_box
