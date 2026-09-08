// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linyaps_box/utils/ns_flags.h"

#include <sched.h>

namespace linyaps_box {
namespace {

using namespace linyaps_box;
using namespace linyaps_box::config; // NOLINT

TEST(NsFlags, NoNamespacesYieldsSigchldOnly)
{
    EXPECT_EQ(utils::generate_clone_flags(std::nullopt), static_cast<unsigned>(SIGCHLD));
}

TEST(NsFlags, EmptyNamespacesListYieldsSigchldOnly)
{
    EXPECT_EQ(utils::generate_clone_flags(std::vector<ns>{ }), static_cast<unsigned>(SIGCHLD));
}

TEST(NsFlags, UnpathedNamespacesProduceCloneFlags)
{
    const std::vector<ns> namespaces{ { config::ns::type::pid, std::nullopt },
                                      { config::ns::type::mount, std::nullopt } };
    const auto flags = utils::generate_clone_flags(namespaces);
    EXPECT_EQ(flags, static_cast<unsigned>(SIGCHLD | CLONE_NEWPID | CLONE_NEWNS));
}

TEST(NsFlags, PathedNamespaceExcludedFromCloneFlags)
{
    const std::vector<ns> namespaces{ { config::ns::type::pid, "/proc/self/ns/pid" },
                                      { config::ns::type::mount, std::nullopt } };
    const auto flags = utils::generate_clone_flags(namespaces);
    EXPECT_EQ(flags, static_cast<unsigned>(SIGCHLD | CLONE_NEWNS));
    EXPECT_EQ(flags & CLONE_NEWPID, 0U);
}

TEST(NsFlags, AllPathedYieldsSigchldOnly)
{
    const std::vector<ns> namespaces{ { config::ns::type::pid, "/proc/self/ns/pid" },
                                      { config::ns::type::mount, "/proc/self/ns/mnt" } };
    EXPECT_EQ(utils::generate_clone_flags(namespaces), static_cast<unsigned>(SIGCHLD));
}

} // namespace
} // namespace linyaps_box
