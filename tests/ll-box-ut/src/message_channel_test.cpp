// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/infra/unix_socket.h"
#include "linyaps_box/protocol/message.h"
#include "linyaps_box/protocol/message_channel.h"
#include "linyaps_box/utils/span.h"

#include <thread>

#include <sys/stat.h>
#include <unistd.h>

namespace {

TEST(MessageChannel, SerializeDie)
{
    linyaps_box::protocol::msg::die original{ 13, "permission denied" };
    auto bytes =
      linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
    auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::die>(deserialized));
    auto &d = std::get<linyaps_box::protocol::msg::die>(deserialized);
    EXPECT_EQ(d.errnum, 13);
    EXPECT_EQ(d.message, "permission denied");

    // wire format: [msg_id(1)][errnum(4)][message_string]
    EXPECT_EQ(bytes.size(), 1 + sizeof(int) + 17);
    EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::die));
}

TEST(MessageChannel, EmptyDieMessage)
{
    linyaps_box::protocol::msg::die original{ 0, "" };
    auto bytes =
      linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
    auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::die>(deserialized));
    auto &d = std::get<linyaps_box::protocol::msg::die>(deserialized);
    EXPECT_EQ(d.errnum, 0);
    EXPECT_TRUE(d.message.empty());

    // wire format: [msg_id(1)][errnum(4)] — no message payload
    EXPECT_EQ(bytes.size(), 1 + sizeof(int));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::die));
}

TEST(MessageChannel, SerializeAllStages)
{
    using linyaps_box::protocol::stage::type;

    auto all_stages = {
        type::namespace_ready,      type::namespace_done,      type::prestart_ready,
        type::prestart_done,        type::createruntime_ready, type::createruntime_done,
        type::createcontainer_done, type::exec_ready,
    };

    bool first{ true };
    for (auto s : all_stages) {
        linyaps_box::protocol::msg::stage original{ s };
        auto bytes =
          linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
        auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

        ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::stage>(deserialized));
        EXPECT_EQ(std::get<linyaps_box::protocol::msg::stage>(deserialized).value, s);

        // wire format: [msg_id(1)][stage_value(1)]
        EXPECT_EQ(bytes.size(), 1 + 1);
        EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::stage));
        if (first) {
            EXPECT_EQ(bytes[1],
                      static_cast<std::byte>(
                        static_cast<std::underlying_type_t<linyaps_box::protocol::stage::type>>(
                          linyaps_box::protocol::stage::type::namespace_ready)));
            first = false;
        }
    }
}

TEST(MessageChannel, UnknownMsgIdThrows)
{
    std::vector<std::byte> wire(1);
    wire[0] = static_cast<std::byte>(42);
    EXPECT_THROW(
      {
          try {
              std::ignore = linyaps_box::protocol::msg::deserialize(wire);
          } catch (const std::runtime_error &e) {
              EXPECT_NE(std::string(e.what()).find("42"), std::string::npos);
              throw;
          }
      },
      std::runtime_error);
}

TEST(MessageChannel, SerializePidReport)
{
    linyaps_box::protocol::msg::pid_report original{ 12345 };
    auto bytes =
      linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
    auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::pid_report>(deserialized));
    EXPECT_EQ(std::get<linyaps_box::protocol::msg::pid_report>(deserialized).value, 12345);

    // wire format: [msg_id(1)][pid_t(sizeof(pid_t))]
    EXPECT_EQ(bytes.size(), 1 + sizeof(pid_t));
    EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::pid_report));
}

TEST(MessageChannel, ChildToParentPidReport)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    child.send(linyaps_box::protocol::msg::pid_report{ 42 });

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::pid_report>(inc.body));
    EXPECT_EQ(std::get<linyaps_box::protocol::msg::pid_report>(inc.body).value, 42);
}

TEST(MessageChannel, SendRecvStage)
{
    auto pair = linyaps_box::protocol::create_message_socketpair();
    auto &parent = pair.first;
    auto &child = pair.second;

    std::thread t([&]() {
        child.wait_for(linyaps_box::protocol::stage::type::namespace_ready);
        child.send_stage(linyaps_box::protocol::stage::type::namespace_done);
    });

    parent.send_stage(linyaps_box::protocol::stage::type::namespace_ready);
    parent.wait_for(linyaps_box::protocol::stage::type::namespace_done);

    t.join();
}

TEST(MessageChannel, SendRecvDie)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    child.send(linyaps_box::protocol::msg::die{ 5, "test error" });

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::die>(inc.body));
    auto &d = std::get<linyaps_box::protocol::msg::die>(inc.body);
    EXPECT_EQ(d.errnum, 5);
    EXPECT_EQ(d.message, "test error");
    EXPECT_TRUE(inc.fds.empty());
}

TEST(MessageChannel, WaitForThrowsOnDie)
{
    auto pair = linyaps_box::protocol::create_message_socketpair();
    auto &parent = pair.first;
    auto &child = pair.second;

    std::thread t([&]() {
        child.send(linyaps_box::protocol::msg::die{ 22, "fatal" });
    });

    EXPECT_THROW(parent.wait_for(linyaps_box::protocol::stage::type::namespace_done),
                 std::system_error);
    t.join();
}

TEST(MessageChannel, WaitForUnexpectedStageThrows)
{
    auto pair = linyaps_box::protocol::create_message_socketpair();
    auto &parent = pair.first;
    auto &child = pair.second;

    std::thread t([&]() {
        child.send_stage(linyaps_box::protocol::stage::type::namespace_ready);
    });

    EXPECT_THROW(parent.wait_for(linyaps_box::protocol::stage::type::createcontainer_done),
                 std::runtime_error);
    t.join();
}

TEST(MessageChannel, TakeFdFromIncoming)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();
    auto [a, b] = linyaps_box::infra::unix_socket::create_socketpair();
    auto b_fd = b.release();

    std::vector<linyaps_box::utils::file_descriptor> fds;
    fds.emplace_back(b_fd, true);
    child.send(
      linyaps_box::protocol::msg::stage{ linyaps_box::protocol::stage::type::namespace_ready },
      fds);

    auto inc = parent.recv();
    ASSERT_FALSE(inc.fds.empty());

    auto rec_fds = inc.take_fds();
    EXPECT_TRUE(!rec_fds.empty() && rec_fds.size() == 1);
    EXPECT_TRUE(inc.fds.empty());

    // verify the received fd is the same socket endpoint by write/read round-trip
    char wbuf = 'X';
    char rbuf = 0;
    EXPECT_EQ(write(a.fd().get(), &wbuf, 1), 1);
    EXPECT_EQ(read(rec_fds[0].get(), &rbuf, 1), 1);
    EXPECT_EQ(rbuf, 'X');
}

TEST(MessageChannel, TakeFdEmptyThrows)
{
    linyaps_box::protocol::msg::datagram inc;
    inc.body =
      linyaps_box::protocol::msg::stage{ linyaps_box::protocol::stage::type::namespace_ready };
    EXPECT_THROW(std::ignore = inc.take_fds(), std::runtime_error);
}

TEST(MessageChannel, UnknownStageTypeThrows)
{
    // msg_id::stage followed by an invalid stage byte (0xFF)
    std::vector<std::byte> wire = { static_cast<std::byte>(linyaps_box::protocol::msg_id::stage),
                                    std::byte{ 0xFF } };
    EXPECT_THROW(
      {
          try {
              std::ignore = linyaps_box::protocol::msg::deserialize(wire);
          } catch (const std::runtime_error &e) {
              EXPECT_NE(std::string(e.what()).find("255"), std::string::npos);
              throw;
          }
      },
      std::runtime_error);
}

TEST(MessageChannel, WaitForExecSocketCloseThrows)
{
    auto [p1, p2] = linyaps_box::infra::unix_socket::create_socketpair();
    p2.close();
    const linyaps_box::protocol::parent_message_channel parent(std::move(p1));

    EXPECT_THROW(parent.wait_for_exec(), std::runtime_error);
}

TEST(MessageChannel, WaitForExecWithExecReady)
{
    auto [s1, s2] = linyaps_box::infra::unix_socket::create_socketpair();
    linyaps_box::protocol::parent_message_channel parent(std::move(s1));

    auto msg = linyaps_box::protocol::msg::serialize(
      linyaps_box::protocol::msg::stage{ linyaps_box::protocol::stage::type::exec_ready });
    s2.send(linyaps_box::utils::span<const std::byte>(msg.data(), msg.size()));
    s2.close();

    EXPECT_NO_THROW(parent.wait_for_exec());
}

TEST(MessageChannel, WaitForExecFailedAfterReady)
{
    auto pair = linyaps_box::protocol::create_message_socketpair();
    auto &parent = pair.first;
    auto &child = pair.second;

    std::thread t([&]() {
        child.send_stage(linyaps_box::protocol::stage::type::exec_ready);
        child.send(linyaps_box::protocol::msg::die{ ENOENT, "execvpe" });
    });

    EXPECT_THROW(
      {
          try {
              parent.wait_for_exec();
          } catch (const std::system_error &e) {
              EXPECT_EQ(e.code().value(), ENOENT);
              EXPECT_TRUE(std::string(e.what()).find("execvpe") != std::string::npos);
              throw;
          }
      },
      std::system_error);
    t.join();
}

TEST(MessageChannel, WaitForExecDie)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    child.send(linyaps_box::protocol::msg::die{ 1, "bad exec" });

    EXPECT_THROW(
      {
          try {
              parent.wait_for_exec();
          } catch (const std::system_error &e) {
              EXPECT_EQ(e.code().value(), 1);
              throw;
          }
      },
      std::system_error);
}

TEST(MessageChannel, WaitForExecUnexpectedMsgThrows)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    child.send_stage(linyaps_box::protocol::stage::type::namespace_ready);

    EXPECT_THROW(parent.wait_for_exec(), std::runtime_error);
}

TEST(MessageChannel, ChildWaitForDie)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    parent.send(linyaps_box::protocol::msg::die{ 5, "parent error" });

    EXPECT_THROW(
      {
          try {
              child.wait_for(linyaps_box::protocol::stage::type::namespace_ready);
          } catch (const std::system_error &e) {
              EXPECT_EQ(e.code().value(), static_cast<int>(EPROTO));
              throw;
          }
      },
      std::system_error);
}

TEST(MessageChannel, LargeDieMessageRoundTrip)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    const std::string long_msg(5000, 'x');
    child.send(linyaps_box::protocol::msg::die{ 99, long_msg });

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::die>(inc.body));
    auto &d = std::get<linyaps_box::protocol::msg::die>(inc.body);
    EXPECT_EQ(d.errnum, 99);
    EXPECT_EQ(d.message, long_msg);
}

TEST(MessageChannel, FdsExceedLimitThrows)
{
    linyaps_box::protocol::msg::stage msg{ linyaps_box::protocol::stage::type::namespace_ready };

    // Create 17 dummy fds (/dev/null), exceeding kMaxScmFds = 16
    std::vector<linyaps_box::utils::file_descriptor> fds;
    for (int i = 0; i < 17; ++i) {
        auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        fds.emplace_back(fd, true);
    }

    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();
    EXPECT_THROW(child.send(msg, fds), std::logic_error);
}

TEST(MessageChannel, SerializeConsoleFd)
{
    linyaps_box::protocol::msg::console_fd original{ };
    auto bytes =
      linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
    auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::console_fd>(deserialized));

    // wire format: [msg_id(1)]
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::console_fd));
}

TEST(MessageChannel, SerializeProceed)
{
    linyaps_box::protocol::msg::proceed original{ };
    auto bytes =
      linyaps_box::protocol::msg::serialize(linyaps_box::protocol::msg::message{ original });
    auto deserialized = linyaps_box::protocol::msg::deserialize(bytes);

    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::proceed>(deserialized));

    // wire format: [msg_id(1)]
    EXPECT_EQ(bytes.size(), 1);
    EXPECT_EQ(bytes[0], static_cast<std::byte>(linyaps_box::protocol::msg_id::proceed));
}

TEST(MessageChannel, SendRecvConsoleFd)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    auto [a, b] = linyaps_box::infra::unix_socket::create_socketpair();
    auto b_fd = b.release();

    std::vector<linyaps_box::utils::file_descriptor> fds;
    fds.emplace_back(b_fd, true);
    child.send(linyaps_box::protocol::msg::console_fd{ }, fds);

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::console_fd>(inc.body));
    ASSERT_FALSE(inc.fds.empty());
}

TEST(MessageChannel, ReportErrorDie)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    child.report_error(EPERM, "test error from child");

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::die>(inc.body));
    auto &d = std::get<linyaps_box::protocol::msg::die>(inc.body);
    EXPECT_EQ(d.errnum, EPERM);
    EXPECT_EQ(d.message, "test error from child");
    EXPECT_TRUE(inc.fds.empty());
}

TEST(MessageChannel, ReportErrorOnClosedSocket)
{
    // report_error must not throw even when the peer has closed its end
    auto [p1, p2] = linyaps_box::infra::unix_socket::create_socketpair();
    p1.close();
    linyaps_box::protocol::child_message_channel child(std::move(p2));
    EXPECT_NO_THROW(child.report_error(EPERM, "parent gone"));
}

TEST(MessageChannel, MaxFdsTransfer)
{
    auto [parent, child] = linyaps_box::protocol::create_message_socketpair();

    std::vector<linyaps_box::utils::file_descriptor> fds;
    for (int i = 0; i < 16; ++i) {
        auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        ASSERT_GE(fd, 0);
        fds.emplace_back(fd, true);
    }

    child.send(
      linyaps_box::protocol::msg::stage{ linyaps_box::protocol::stage::type::namespace_ready },
      fds);

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::stage>(inc.body));
    ASSERT_EQ(inc.fds.size(), 16UL);

    for (const auto &fd : inc.fds) {
        struct stat st{ };
        EXPECT_EQ(fstat(fd.get(), &st), 0);
        EXPECT_TRUE(S_ISCHR(st.st_mode));
    }
}

TEST(MessageChannel, ProceedRoundTrip)
{
    auto pair = linyaps_box::protocol::create_message_socketpair();
    const auto &child = pair.second;
    const auto &parent = pair.first;

    std::thread t([&]() {
        child.send(linyaps_box::protocol::msg::pid_report{ 42 });
        child.wait_for_proceed();
    });

    auto inc = parent.recv();
    ASSERT_TRUE(std::holds_alternative<linyaps_box::protocol::msg::pid_report>(inc.body));
    EXPECT_EQ(std::get<linyaps_box::protocol::msg::pid_report>(inc.body).value, 42);

    parent.send(linyaps_box::protocol::msg::proceed{ });

    t.join();
}

} // namespace
