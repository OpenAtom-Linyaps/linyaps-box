// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/infra/unix_socket.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/log/sinks/sync_socket_sink.h"
#include "linyaps_box/log/utils.h"
#include "linyaps_box/protocol/message.h"
#include "linyaps_box/protocol/message_channel.h"
#include "linyaps_box/utils/span.h"

#include <chrono>
#include <optional>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

namespace {

namespace proto = linyaps_box::protocol;
namespace msg = linyaps_box::protocol::msg;
namespace log_lvl = linyaps_box::log;

auto make_log(log_lvl::level lvl = log_lvl::level::fatal,
              std::string_view message = "test",
              pid_t pid = 0,
              std::chrono::nanoseconds time = { }) -> msg::log
{
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    return { std::string{ message }, "test.cpp", "f", 1, 0, time, pid, lvl };
#else
    return { std::string{ message }, 0, time, pid, lvl };
#endif
}

[[maybe_unused]] auto make_log_context(log_lvl::level lvl = log_lvl::level::fatal,
                                       std::string_view message = "test",
                                       pid_t pid = 0,
                                       std::chrono::nanoseconds time = { }) -> log_lvl::log_context
{
    return {
        lvl,
        std::string{ message },
        std::chrono::system_clock::time_point{
          std::chrono::duration_cast<std::chrono::system_clock::duration>(time) },
        pid,
        0,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        "test.cpp",
        "f",
        1,
#endif
    };
}

auto log_wire_size(const msg::log &l) -> std::size_t
{
    return sizeof(proto::msg_id)            // msg_id
      + sizeof(uint8_t)                     // lvl
      + sizeof(uint32_t) + l.message.size() // string + message
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
      + sizeof(uint32_t) + l.file.size()     // string + file
      + sizeof(int)                          // line
      + sizeof(uint32_t) + l.function.size() // string + function
#endif
      + sizeof(int)      // errno
      + sizeof(pid_t)    // pid
      + sizeof(int64_t); // time
}

class thread_guard
{
public:
    explicit thread_guard(std::thread th)
        : t(std::move(th))
    {
    }

    ~thread_guard()
    {
        if (t.joinable()) {
            t.join();
        }
    }

    thread_guard(const thread_guard &) = delete;
    auto operator=(const thread_guard &) -> thread_guard & = delete;
    thread_guard(thread_guard &&) noexcept = delete;
    auto operator=(thread_guard &&) noexcept -> thread_guard & = delete;

private:
    std::thread t;
};

class ChannelTest : public ::testing::Test
{
protected:
    std::optional<proto::parent_message_channel> parent;
    std::optional<proto::child_message_channel> child;
    linyaps_box::log::level saved_level_;

    void SetUp() override
    {
        auto [p, c] = proto::create_message_socketpair();
        parent.emplace(std::move(p));
        child.emplace(std::move(c));

        auto &logger = linyaps_box::log::global_logger::instance();
        saved_level_ = logger.get_level();
        logger.unset_sink();
    }

    void TearDown() override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        logger.unset_sink();
        logger.set_level(saved_level_);
    }
};

class SerializeStageTest : public ::testing::TestWithParam<proto::stage::type>
{
};

TEST_P(SerializeStageTest, RoundTrip)
{
    msg::stage original{ GetParam() };
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::stage>(deserialized));
    EXPECT_EQ(std::get<msg::stage>(deserialized).value, GetParam());

    // wire format: [msg_id(1)][stage_value(1)]
    EXPECT_EQ(bytes.size(), sizeof(proto::msg_id) + sizeof(proto::stage::type));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::stage));
    EXPECT_EQ(
      bytes[1],
      static_cast<std::byte>(static_cast<std::underlying_type_t<proto::stage::type>>(GetParam())));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SerializeStageTest,
                         ::testing::Values(proto::stage::type::namespace_ready,
                                           proto::stage::type::namespace_done,
                                           proto::stage::type::prestart_ready,
                                           proto::stage::type::prestart_done,
                                           proto::stage::type::createruntime_ready,
                                           proto::stage::type::createruntime_done,
                                           proto::stage::type::createcontainer_done,
                                           proto::stage::type::exec_ready));

TEST(MessageChannel, SerializeLog)
{
    auto now = std::chrono::system_clock::now().time_since_epoch();
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    msg::log original{ "permission denied",  "test_file", "test", 42, 0, now, 0,
                       log_lvl::level::fatal };
#else
    msg::log original{ "permission denied", 0, now, 0, log_lvl::level::fatal };
#endif
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::log>(deserialized));
    auto &d = std::get<msg::log>(deserialized);
    EXPECT_EQ(d.lvl, log_lvl::level::fatal);
    EXPECT_EQ(d.message, "permission denied");
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_EQ(d.file, "test_file");
    EXPECT_EQ(d.function, "test");
    EXPECT_EQ(d.line, 42);
#endif
    EXPECT_EQ(d.time, now);
    EXPECT_EQ(d.errno_, 0);

    EXPECT_EQ(bytes.size(), log_wire_size(original));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::log));
}

TEST(MessageChannel, EmptyLogMessage)
{
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    msg::log original{ "", "", "", 0, 0, std::chrono::nanoseconds{ 0 }, 0, log_lvl::level::fatal };
#else
    msg::log original{ 0, 0, std::chrono::nanoseconds{ 0 }, 0, log_lvl::level::fatal };
#endif
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::log>(deserialized));
    auto &d = std::get<msg::log>(deserialized);
    EXPECT_EQ(d.lvl, log_lvl::level::fatal);
    EXPECT_TRUE(d.message.empty());
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_TRUE(d.file.empty());
    EXPECT_EQ(d.line, 0);
#endif
    EXPECT_EQ(d.time, std::chrono::nanoseconds{ 0 });

    EXPECT_EQ(bytes.size(), log_wire_size(original));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::log));
}

TEST(MessageChannel, UnknownMsgIdThrows)
{
    std::vector<std::byte> wire(1);
    wire[0] = static_cast<std::byte>(42);
    EXPECT_THROW(
      {
          try {
              std::ignore = msg::deserialize(wire);
          } catch (const std::runtime_error &e) {
              EXPECT_NE(std::string(e.what()).find("42"), std::string::npos);
              throw;
          }
      },
      std::runtime_error);
}

TEST(MessageChannel, SerializePidReport)
{
    msg::pid_report original{ 12345 };
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::pid_report>(deserialized));
    EXPECT_EQ(std::get<msg::pid_report>(deserialized).value, 12345);

    EXPECT_EQ(bytes.size(), sizeof(proto::msg_id) + sizeof(pid_t));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::pid_report));
}

TEST(MessageChannel, TakeFdEmptyThrows)
{
    msg::datagram inc;
    inc.body = msg::stage{ proto::stage::type::namespace_ready };
    EXPECT_THROW(std::ignore = inc.take_fds(), std::runtime_error);
}

TEST(MessageChannel, UnknownStageTypeThrows)
{
    std::vector<std::byte> wire = { static_cast<std::byte>(proto::msg_id::stage),
                                    std::byte{ 0xFF } };
    EXPECT_THROW(
      {
          try {
              std::ignore = msg::deserialize(wire);
          } catch (const std::runtime_error &e) {
              EXPECT_NE(std::string(e.what()).find("255"), std::string::npos);
              throw;
          }
      },
      std::runtime_error);
}

TEST(MessageChannel, SerializeConsoleFd)
{
    msg::console_fd original{ };
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::console_fd>(deserialized));

    EXPECT_EQ(bytes.size(), sizeof(proto::msg_id));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::console_fd));
}

TEST(MessageChannel, SerializeProceed)
{
    msg::proceed original{ };
    auto bytes = msg::serialize(msg::message{ original });
    auto deserialized = msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<msg::proceed>(deserialized));

    EXPECT_EQ(bytes.size(), sizeof(proto::msg_id));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(proto::msg_id::proceed));
}

// ── socketpair tests ───────────────────────────────────────────────

TEST_F(ChannelTest, ChildToParentPidReport)
{
    child->send(msg::pid_report{ 42 });

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::pid_report>(inc.body));
    EXPECT_EQ(std::get<msg::pid_report>(inc.body).value, 42);
}

TEST_F(ChannelTest, SendRecvStage)
{
    const thread_guard tg{ std::thread([&]() {
        child->wait_for(proto::stage::type::namespace_ready);
        child->send_stage(proto::stage::type::namespace_done);
    }) };

    parent->send_stage(proto::stage::type::namespace_ready);
    parent->wait_for(proto::stage::type::namespace_done);
}

TEST_F(ChannelTest, SendRecvLog)
{
    child->send(make_log(log_lvl::level::fatal, "test error"));

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::log>(inc.body));
    auto &d = std::get<msg::log>(inc.body);
    EXPECT_EQ(d.lvl, log_lvl::level::fatal);
    EXPECT_EQ(d.message, "test error");
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_EQ(d.file, "test.cpp");
    EXPECT_EQ(d.line, 1);
#endif
    EXPECT_EQ(d.time, std::chrono::nanoseconds{ 0 });
    EXPECT_TRUE(inc.fds.empty());
}

TEST_F(ChannelTest, WaitForUnexpectedStageThrows)
{
    const thread_guard tg{ std::thread([&]() {
        child->send_stage(proto::stage::type::namespace_ready);
    }) };

    EXPECT_THROW(parent->wait_for(proto::stage::type::createcontainer_done), std::runtime_error);
}

TEST_F(ChannelTest, TakeFdFromIncoming)
{
    auto [a, b] = linyaps_box::infra::unix_socket::create_socketpair();
    auto b_fd = b.release();

    std::vector<linyaps_box::utils::file_descriptor> fds;
    fds.emplace_back(b_fd, true);
    child->send(msg::stage{ proto::stage::type::namespace_ready }, fds);

    auto inc = parent->recv();
    ASSERT_FALSE(inc.fds.empty());

    auto rec_fds = inc.take_fds();
    EXPECT_TRUE(!rec_fds.empty() && rec_fds.size() == 1);
    EXPECT_TRUE(inc.fds.empty());

    // verify the received fd is the same socket endpoint by write/read round-trip
    char wbuf = 'X';
    char rbuf = 0;
    ASSERT_EQ(write(a.fd().get(), &wbuf, 1), 1);
    ASSERT_EQ(read(rec_fds[0].get(), &rbuf, 1), 1);
    EXPECT_EQ(rbuf, 'X');
}

TEST_F(ChannelTest, WaitForExecSocketCloseThrows)
{
    auto [p1, p2] = linyaps_box::infra::unix_socket::create_socketpair();
    p2.close();
    const proto::parent_message_channel isolated_parent(std::move(p1));

    EXPECT_THROW(isolated_parent.wait_for(proto::stage::type::exec_ready), std::runtime_error);
}

TEST_F(ChannelTest, WaitForExecWithExecReady)
{
    auto [s1, s2] = linyaps_box::infra::unix_socket::create_socketpair();
    const proto::parent_message_channel isolated_parent(std::move(s1));

    auto msg = msg::serialize(msg::stage{ proto::stage::type::exec_ready });
    s2.send(linyaps_box::utils::span<const std::byte>(msg.data(), msg.size()));
    s2.close();

    EXPECT_NO_THROW(isolated_parent.wait_for(proto::stage::type::exec_ready));
    EXPECT_NO_THROW(isolated_parent.wait_for_close());
}

TEST_F(ChannelTest, WaitForExecFailedAfterReady)
{
    const thread_guard tg{ std::thread([&]() {
        child->send_stage(proto::stage::type::exec_ready);
        child->send(make_log(log_lvl::level::fatal, "execvpe"));
        child.reset();
    }) };

    EXPECT_NO_THROW(parent->wait_for(proto::stage::type::exec_ready));
    EXPECT_THROW(parent->wait_for_close(), std::runtime_error);
}

TEST_F(ChannelTest, ChildWaitForUnexpected)
{
    parent->send(msg::pid_report{ 42 });

    EXPECT_THROW(
      {
          try {
              child->wait_for(proto::stage::type::namespace_ready);
          } catch (const std::runtime_error &e) {
              EXPECT_NE(std::string(e.what()).find("unexpected"), std::string::npos);
              throw;
          }
      },
      std::runtime_error);
}

TEST_F(ChannelTest, LargeLogMessageRoundTrip)
{
    const std::string long_msg(5000, 'x');
    child->send(make_log(log_lvl::level::error, long_msg));

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::log>(inc.body));
    auto &d = std::get<msg::log>(inc.body);
    EXPECT_EQ(d.message, long_msg);
}

TEST_F(ChannelTest, FdsExceedLimitThrows)
{
    msg::stage m{ proto::stage::type::namespace_ready };

    std::vector<linyaps_box::utils::file_descriptor> fds;
    for (int i = 0; i < 17; ++i) {
        auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        ASSERT_GE(fd, 0);
        fds.emplace_back(fd, true);
    }

    EXPECT_THROW(child->send(m, fds), std::logic_error);
}

TEST_F(ChannelTest, SendRecvConsoleFd)
{
    auto [a, b] = linyaps_box::infra::unix_socket::create_socketpair();
    auto b_fd = b.release();

    std::vector<linyaps_box::utils::file_descriptor> fds;
    fds.emplace_back(b_fd, true);
    child->send(msg::console_fd{ }, fds);

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::console_fd>(inc.body));
    ASSERT_FALSE(inc.fds.empty());
}

TEST_F(ChannelTest, SendOnClosedSocketThrows)
{
    auto [p1, p2] = linyaps_box::infra::unix_socket::create_socketpair();
    p1.close();
    const proto::child_message_channel isolated_child(std::move(p2));
    EXPECT_THROW(isolated_child.send(make_log(log_lvl::level::fatal, "parent gone")),
                 std::system_error);
}

TEST_F(ChannelTest, MaxFdsTransfer)
{
    std::vector<linyaps_box::utils::file_descriptor> fds;
    for (int i = 0; i < 16; ++i) {
        auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        ASSERT_GE(fd, 0);
        fds.emplace_back(fd, true);
    }

    child->send(msg::stage{ proto::stage::type::namespace_ready }, fds);

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::stage>(inc.body));
    ASSERT_EQ(inc.fds.size(), 16UL);

    for (const auto &fd : inc.fds) {
        struct stat st{ };
        EXPECT_EQ(fstat(fd.get(), &st), 0);
        EXPECT_TRUE(S_ISCHR(st.st_mode));
    }
}

TEST_F(ChannelTest, ProceedRoundTrip)
{
    const thread_guard tg{ std::thread([&]() {
        child->send(msg::pid_report{ 42 });
        child->wait_for_proceed();
    }) };

    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::pid_report>(inc.body));
    EXPECT_EQ(std::get<msg::pid_report>(inc.body).value, 42);

    parent->send(msg::proceed{ });
}

TEST_F(ChannelTest, SyncSocketSinkForwardsLogToParent)
{
    // Install a sync_socket_sink on the child side.  The child's log call
    // should be serialized, sent over the socket, and arrive at the parent.
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.unset_sink();
    logger.set_sink(linyaps_box::log::sync_socket_sink{ *child });
    logger.set_level(linyaps_box::log::level::debug);

    constexpr auto expected_line = __LINE__ + 1;
    LINYAPS_BOX_LOG_ERROR("sync_sink_test {} {}", 42, "hello");

    // Read back on the parent side via drain_logs.
    auto inc = parent->recv();
    ASSERT_TRUE(std::holds_alternative<msg::log>(inc.body));
    auto &d = std::get<msg::log>(inc.body);
    EXPECT_EQ(d.lvl, log_lvl::level::error);
    EXPECT_EQ(d.message, "sync_sink_test 42 hello");
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_NE(d.file.find("message_channel_test.cpp"), std::string::npos);
    EXPECT_EQ(d.line, expected_line);
#endif
    EXPECT_GT(d.time.count(), 0);

    logger.unset_sink();
    logger.set_level(linyaps_box::log::level::warn);
}

TEST_F(ChannelTest, WaitForDrainsInterleavedLogs)
{
    const thread_guard tg{ std::thread([&]() {
        child->send(make_log(log_lvl::level::debug, "dbg1"));
        child->send(make_log(log_lvl::level::info, "info1"));
        child->send_stage(proto::stage::type::namespace_ready);
    }) };

    EXPECT_NO_THROW(parent->wait_for(proto::stage::type::namespace_ready));
}

TEST_F(ChannelTest, WaitForDrainsLogsThenCloseThrows)
{
    const thread_guard tg{ std::thread([&]() {
        child->send(make_log(log_lvl::level::error, "boom"));
        child.reset();
    }) };

    EXPECT_THROW(parent->wait_for(proto::stage::type::exec_ready), std::runtime_error);
}

TEST_F(ChannelTest, DrainLogsReturnsNonLogDatagram)
{
    const thread_guard tg{ std::thread([&]() {
        child->send(make_log(log_lvl::level::info, "ignored"));
        child->send(make_log(log_lvl::level::debug, "also ignored"));
        child->send(msg::pid_report{ 7 });
    }) };

    auto inc = parent->drain_logs();
    ASSERT_TRUE(std::holds_alternative<msg::pid_report>(inc.body));
    EXPECT_EQ(std::get<msg::pid_report>(inc.body).value, 7);
}

TEST_F(ChannelTest, DrainLogsThrowsOnClose)
{
    const thread_guard tg{ std::thread([&]() {
        child->send(make_log(log_lvl::level::info, "x"));
        child.reset();
    }) };

    EXPECT_THROW(std::ignore = parent->drain_logs(), std::runtime_error);
}

TEST_F(ChannelTest, WaitForCloseThrowsSystemErrorWithErrno)
{
    const thread_guard tg{ std::thread([&]() {
        child->send_stage(proto::stage::type::exec_ready);
        auto l = make_log(log_lvl::level::fatal, "execvpe");
        l.errno_ = ENOENT;
        child->send(l);
        child.reset();
    }) };

    EXPECT_NO_THROW(parent->wait_for(proto::stage::type::exec_ready));
    try {
        parent->wait_for_close();
        FAIL() << "expected throw";
    } catch (const std::system_error &e) {
        EXPECT_EQ(e.code().value(), ENOENT);
    }
}

} // namespace
