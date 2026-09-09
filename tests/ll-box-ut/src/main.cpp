// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/sinks/stderr_sink.h"

#include <sys/prctl.h>

#include <memory>
#include <vector>

namespace {

// The global logger is process-wide state shared by every test. Some fixtures
// (e.g. LogFixture, ChannelTest) call unset_backend() as part of their teardown
// and leave the logger with no backend installed, so any later test that logs
// would hit dispatch_log()'s "logger uninitialized" path and std::terminate().
// Reinstall a default stderr sink before each test so a leaked monostate
// backend can never crash a subsequent test.
class default_log_backend_guard : public testing::EmptyTestEventListener
{
public:
    void OnTestStart([[maybe_unused]] const testing::TestInfo &info) override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
        sinks.push_back(
          std::make_unique<linyaps_box::log::stderr_sink>(linyaps_box::log::stderr_spec{ },
                                                          linyaps_box::log::output_format::text));
        logger.set_sinks(std::move(sinks));
    }
};

} // anonymous namespace

int main(int argc, char **argv)
{

    testing::InitGoogleTest(&argc, argv);

    testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new default_log_backend_guard);

    // prevent death test generates coredump
    prctl(PR_SET_DUMPABLE, 0);

    auto result = RUN_ALL_TESTS();

    return result;
}
