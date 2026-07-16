// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linyaps_box/log/formatter.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/log/sink.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>

namespace {

auto make_ctx(linyaps_box::log::level lvl = linyaps_box::log::level::error,
              std::string_view msg = "test",
              std::string_view file = "test.cpp",
              std::string_view function = "fn",
              int line = 1) -> linyaps_box::log::log_context
{
    return { lvl,  msg,      { }, 0, 0,
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
             file, function, line
#endif
    };
}

struct mock_syslog_backend
{
    struct call
    {
        int priority;
        std::string msg;
    };

    mutable std::vector<call> calls;

    explicit mock_syslog_backend(std::string /*ident*/) { }

    void syslog(linyaps_box::log::level priority, std::string_view msg) const noexcept
    {
        calls.push_back({ linyaps_box::log::to_syslog_priority(priority), std::string{ msg } });
    }
};

using mock_syslog_sink = linyaps_box::log::basic_syslog_sink<mock_syslog_backend>;

struct mock_syslog_spec
{
    std::string ident;
    bool cee{ false };
};

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION

struct mock_journald_backend
{
    struct call
    {
        std::vector<std::string> fields;
    };

    static inline std::vector<call> calls{ };

    static auto send(linyaps_box::utils::span<const struct iovec> iov) noexcept -> void
    {
        call c;
        for (std::size_t i = 0; i < iov.size(); ++i) {
            c.fields.emplace_back(static_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
        }
        calls.push_back(std::move(c));
    }
};

using mock_journald_sink = linyaps_box::log::basic_journald_sink<mock_journald_backend>;

struct mock_journald_spec
{
    std::string ident;
};

#endif

struct stderr_capture
{
    int saved_fd{ -1 };
    std::array<int, 2> pipe_fd{ };

    stderr_capture(const stderr_capture &) = delete;
    stderr_capture(stderr_capture &&) noexcept = delete;
    stderr_capture &operator=(const stderr_capture &) = delete;
    stderr_capture &operator=(stderr_capture &&) = delete;

    stderr_capture()
    {
        if (::pipe(pipe_fd.data()) < 0) {
            throw std::system_error(errno, std::system_category(), "stderr_capture: pipe failed");
        }

        saved_fd = ::dup(STDERR_FILENO);
        if (saved_fd < 0) {
            ::close(pipe_fd[0]);
            ::close(pipe_fd[1]);
            throw std::system_error(errno, std::system_category(), "stderr_capture: dup failed");
        }

        ::dup2(pipe_fd[1], STDERR_FILENO);
        ::close(pipe_fd[1]);
        pipe_fd[1] = -1;
    }

    ~stderr_capture()
    {
        if (saved_fd >= 0) {
            ::dup2(saved_fd, STDERR_FILENO);
            ::close(saved_fd);
        }

        if (pipe_fd[0] >= 0) {
            ::close(pipe_fd[0]);
        }
    }

    auto extract() -> std::string
    {
        if (::fcntl(STDERR_FILENO, F_GETFD) >= 0) {
            ::close(STDERR_FILENO);
        }

        std::string result;
        std::array<char, 4096> buf{ };

        while (true) {
            auto n = ::read(pipe_fd[0], buf.data(), buf.size());
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }

                break;
            }

            if (n == 0) {
                break;
            }

            result.append(buf.data(), static_cast<std::size_t>(n));
        }

        return result;
    }
};

class LogFixture : public ::testing::Test
{
protected:
    linyaps_box::log::level saved_level_;
    linyaps_box::log::output_format saved_format_;

    void SetUp() override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        saved_level_ = logger.get_level();
        saved_format_ = logger.get_format();
        logger.unset_sink();
    }

    void TearDown() override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        logger.unset_sink();
        logger.set_level(saved_level_);
        logger.set_format(saved_format_);
    }
};

struct SyslogCase
{
    linyaps_box::log::level lvl;
    int expected;
};

class SyslogPriorityTest : public ::testing::TestWithParam<SyslogCase>
{
};

TEST_P(SyslogPriorityTest, Mapping)
{
    EXPECT_EQ(linyaps_box::log::to_syslog_priority(GetParam().lvl), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SyslogPriorityTest,
                         ::testing::Values(SyslogCase{ linyaps_box::log::level::fatal, LOG_CRIT },
                                           SyslogCase{ linyaps_box::log::level::error, LOG_ERR },
                                           SyslogCase{ linyaps_box::log::level::warn, LOG_WARNING },
                                           SyslogCase{ linyaps_box::log::level::info, LOG_INFO },
                                           SyslogCase{ linyaps_box::log::level::debug,
                                                       LOG_DEBUG }));

struct LevelNameCase
{
    linyaps_box::log::level lvl;
    std::string_view expected;
};

class LevelNameTest : public ::testing::TestWithParam<LevelNameCase>
{
};

TEST_P(LevelNameTest, Mapping)
{
    EXPECT_EQ(linyaps_box::log::level_name(GetParam().lvl), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LevelNameTest,
                         ::testing::Values(LevelNameCase{ linyaps_box::log::level::fatal, "FATAL" },
                                           LevelNameCase{ linyaps_box::log::level::error, "ERROR" },
                                           LevelNameCase{ linyaps_box::log::level::warn, "WARN" },
                                           LevelNameCase{ linyaps_box::log::level::info, "INFO" },
                                           LevelNameCase{ linyaps_box::log::level::debug,
                                                          "DEBUG" }));

struct FilterCase
{
    linyaps_box::log::level msg_lvl;
    linyaps_box::log::level current_lvl;
    bool expect_output;
};

class LevelFilterTest : public ::testing::TestWithParam<FilterCase>
{
};

TEST_P(LevelFilterTest, Filtering)
{
    auto &logger = linyaps_box::log::global_logger::instance();
    auto saved_level = logger.get_level();
    logger.unset_sink();

    stderr_capture cap;
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(GetParam().current_lvl);

    switch (GetParam().msg_lvl) {
    case linyaps_box::log::level::fatal:
        LINYAPS_BOX_LOG_FATAL("test");
        break;
    case linyaps_box::log::level::error:
        LINYAPS_BOX_LOG_ERROR("test");
        break;
    case linyaps_box::log::level::warn:
        LINYAPS_BOX_LOG_WARN("test");
        break;
    case linyaps_box::log::level::info:
        LINYAPS_BOX_LOG_INFO("test");
        break;
    case linyaps_box::log::level::debug:
        LINYAPS_BOX_LOG_DEBUG("test");
        break;
    }

    auto output = cap.extract();
    EXPECT_EQ(!output.empty(), GetParam().expect_output);

    logger.unset_sink();
    logger.set_level(saved_level);
}

INSTANTIATE_TEST_SUITE_P(
  All,
  LevelFilterTest,
  ::testing::Values(
    FilterCase{ linyaps_box::log::level::fatal, linyaps_box::log::level::error, true },
    FilterCase{ linyaps_box::log::level::error, linyaps_box::log::level::error, true },
    FilterCase{ linyaps_box::log::level::warn, linyaps_box::log::level::error, false },
    FilterCase{ linyaps_box::log::level::info, linyaps_box::log::level::warn, false },
    FilterCase{ linyaps_box::log::level::debug, linyaps_box::log::level::info, false }));

TEST_F(LogFixture, StderrSinkBasicOutput)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(linyaps_box::log::level::debug);

    LINYAPS_BOX_LOG_INFO("hello {}", 42);

    auto output = cap.extract();
    EXPECT_NE(output.find("hello 42"), std::string::npos);
    EXPECT_NE(output.find("[INFO ]"), std::string::npos);
}

#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
TEST_F(LogFixture, SourceLocationPropagation)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(linyaps_box::log::level::debug);

    LINYAPS_BOX_LOG_INFO("loc");

    auto output = cap.extract();
    EXPECT_NE(output.find("log_test.cpp"), std::string::npos);
    EXPECT_NE(output.find(__func__), std::string::npos);
}
#endif

TEST_F(LogFixture, FileSinkOutput)
{
    std::string tmp{ "/tmp/ll_box_test_XXXXXX" };
    const auto fd = ::mkstemp(const_cast<char *>(tmp.data()));
    ASSERT_GE(fd, 0);
    ::close(fd);

    auto &logger = linyaps_box::log::global_logger::instance();
    linyaps_box::log::file_sink sink(linyaps_box::log::file_spec{ tmp });
    logger.set_sink(std::move(sink));
    logger.set_level(linyaps_box::log::level::debug);

    LINYAPS_BOX_LOG_INFO("file test {}", 99);

    std::ifstream in(tmp);
    std::string line;
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_NE(line.find("[INFO ]"), std::string::npos);
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_NE(line.find("file test 99"), std::string::npos);
    EXPECT_NE(line.find("    file test 99"), std::string::npos);

    std::filesystem::remove(tmp);
}

TEST(LogConfig, ParseLogTo)
{
    using namespace linyaps_box::log;

    auto s1 = parse_log_to("stderr");
    EXPECT_TRUE(std::holds_alternative<stderr_spec>(s1));

    auto s2 = parse_log_to("file:/var/log/test.log");
    ASSERT_TRUE(std::holds_alternative<file_spec>(s2));
    EXPECT_EQ(std::get<file_spec>(s2).path, "/var/log/test.log");

    auto s3 = parse_log_to("syslog:myapp");
    ASSERT_TRUE(std::holds_alternative<syslog_spec>(s3));
    EXPECT_EQ(std::get<syslog_spec>(s3).ident, "myapp");

    auto s4 = parse_log_to("/var/log/default.log");
    ASSERT_TRUE(std::holds_alternative<file_spec>(s4));
    EXPECT_EQ(std::get<file_spec>(s4).path, "/var/log/default.log");

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION

    auto s5 = linyaps_box::log::parse_log_to("journald:app");
    ASSERT_TRUE(std::holds_alternative<linyaps_box::log::journald_spec>(s5));
    EXPECT_EQ(std::get<linyaps_box::log::journald_spec>(s5).ident, "app");

#endif
}

TEST_F(LogFixture, DispatchRaw)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });

    auto ctx = make_ctx(linyaps_box::log::level::error, "raw dispatch", "test.cpp", "fn", 42);
    logger.dispatch_raw(ctx);

    auto output = cap.extract();
    EXPECT_NE(output.find("raw dispatch"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_NE(output.find("test.cpp:42"), std::string::npos);
#endif
}

TEST(SyslogSink, CeePrefixInJsonMode)
{
    auto &logger = linyaps_box::log::global_logger::instance();
    auto saved_format = logger.get_format();
    logger.set_format(linyaps_box::log::output_format::json);

    const mock_syslog_sink sink(mock_syslog_spec{ "test_app", true });
    sink.log(make_ctx(linyaps_box::log::level::error, "denied", "auth.cpp", "login", 42));

    ASSERT_EQ(sink.backend().calls.size(), 1U);
    EXPECT_EQ(sink.backend().calls[0].msg.substr(0, 6), "@cee: ");

    auto json = sink.backend().calls[0].msg.substr(6);
    EXPECT_NE(json.find(R"("msg":"denied")"), std::string::npos);
    EXPECT_NE(json.find(R"("level":"ERROR")"), std::string::npos);

    logger.set_format(saved_format);
}

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION

TEST(JournaldSink, PassesAllFields)
{
    mock_journald_backend::calls.clear();
    const mock_journald_sink sink(mock_journald_spec{ "myapp" });
    auto ctx = make_ctx(linyaps_box::log::level::fatal, "segfault", "crash.c", "deref", 77);
    ctx.errno_ = 13;
    sink.log(ctx);

    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    const auto &fields = mock_journald_backend::calls[0].fields;
    EXPECT_EQ(fields[0], "MESSAGE=segfault");
    EXPECT_EQ(fields[1], "PRIORITY=2");
    EXPECT_EQ(fields[2], "SYSLOG_IDENTIFIER=myapp");
#  ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_EQ(fields[3], "CODE_FILE=crash.c");
    EXPECT_EQ(fields[4], "CODE_LINE=77");
    EXPECT_EQ(fields[5], "CODE_FUNC=deref");
    EXPECT_EQ(fields[6], "ERRNO=13");
#  else
    EXPECT_EQ(fields[3], "ERRNO=13");
#  endif
}

#endif

TEST_F(LogFixture, MultiSinkOutput)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();

    std::string tmp{ "/tmp/ll_box_test_XXXXXX" };
    const auto fd = ::mkstemp(const_cast<char *>(tmp.data()));
    ASSERT_GE(fd, 0);
    ::close(fd);

    std::vector<linyaps_box::log::sink_variant> sinks;
    sinks.push_back(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    sinks.push_back(linyaps_box::log::file_sink{ linyaps_box::log::file_spec{ tmp } });
    logger.set_sinks(std::move(sinks));
    logger.set_level(linyaps_box::log::level::info);

    LINYAPS_BOX_LOG_INFO("multi sink test {}", 42);

    auto output = cap.extract();
    EXPECT_NE(output.find("multi sink test 42"), std::string::npos);
    EXPECT_NE(output.find("[INFO ]"), std::string::npos);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_NE(output.find("log_test.cpp"), std::string::npos);
#endif

    std::ifstream in(tmp);
    const std::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("multi sink test 42"), std::string::npos);
    EXPECT_NE(content.find("[INFO ]"), std::string::npos);

    std::filesystem::remove(tmp);
    logger.unset_sink();
}

TEST_F(LogFixture, JsonFormatOutput)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(linyaps_box::log::level::info);
    logger.set_format(linyaps_box::log::output_format::json);

    LINYAPS_BOX_LOG_INFO("json test {}", 42);

    auto output = cap.extract();
    EXPECT_NE(output.find("\"level\":\"INFO\""), std::string::npos);
    EXPECT_NE(output.find("\"msg\":\"json test 42\""), std::string::npos);
    EXPECT_NE(output.find("\"pid\":"), std::string::npos);
    EXPECT_NE(output.find("Z\""), std::string::npos);
    EXPECT_NE(output.find("}\n"), std::string::npos);
}

TEST_F(LogFixture, ErrnoPropagationText)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(linyaps_box::log::level::error);

    LINYAPS_BOX_LOG_ERROR_ERRNO(13, "operation failed");

    auto output = cap.extract();
    EXPECT_NE(output.find("operation failed"), std::string::npos);
    EXPECT_NE(output.find("Permission denied"), std::string::npos);
}

TEST_F(LogFixture, ErrnoPropagationJson)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    logger.set_sink(linyaps_box::log::stderr_sink{ linyaps_box::log::stderr_spec{ } });
    logger.set_level(linyaps_box::log::level::error);
    logger.set_format(linyaps_box::log::output_format::json);

    LINYAPS_BOX_LOG_ERROR_ERRNO(13, "operation failed");

    auto output = cap.extract();
    EXPECT_NE(output.find("\"msg\":\"operation failed\""), std::string::npos);
    EXPECT_NE(output.find("\"errno\":13"), std::string::npos);
    EXPECT_NE(output.find("\"strerror\":\"Permission denied\""), std::string::npos);
}

auto make_ctx_with_time(std::chrono::system_clock::time_point tp,
                        linyaps_box::log::level lvl,
                        std::string_view msg,
                        [[maybe_unused]] std::string_view file = "test.cpp",
                        [[maybe_unused]] std::string_view function = "fn",
                        [[maybe_unused]] int line = 1,
                        int errno_val = 0) -> linyaps_box::log::log_context
{
    linyaps_box::log::log_context ctx{ };
    ctx.lvl = lvl;
    ctx.msg = msg;
    ctx.wall_time = tp;
    ctx.pid = 0;
    ctx.errno_ = errno_val;
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    ctx.file = file;
    ctx.function = function;
    ctx.line = line;
#endif
    return ctx;
}

auto format_to_string(const linyaps_box::log::log_context &ctx, linyaps_box::log::output_format fmt)
  -> std::string
{
    fmt::memory_buffer buf;
    linyaps_box::log::format_log(buf, ctx, fmt, { });
    return { buf.data(), buf.size() };
}

TEST(FormatLog, TextBasic)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::error, "hello");
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::text);
    EXPECT_NE(out.find("[ERROR]"), std::string::npos);
    EXPECT_NE(out.find("hello"), std::string::npos);
}

#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
TEST(FormatLog, TextSourceLocation)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::warn, "msg", "f.cpp", "g", 7);
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::text);
    EXPECT_NE(out.find("f.cpp:7 g"), std::string::npos);
}
#endif

TEST(FormatLog, TextErrno)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::error, "fail", "", "", 0, 13);
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::text);
    EXPECT_NE(out.find("Permission denied"), std::string::npos);
}

TEST(FormatLog, TextMultilineIndent)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::info, "line1\nline2");
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::text);
    // second line should be prefixed with base_indent (4 spaces)
    EXPECT_NE(out.find("\n    line2"), std::string::npos);
}

TEST(FormatLog, JsonFields)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::info, "payload");
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::json);
    EXPECT_NE(out.find(R"("level":"INFO")"), std::string::npos);
    EXPECT_NE(out.find(R"("msg":"payload")"), std::string::npos);
    EXPECT_NE(out.find(R"("pid":)"), std::string::npos);
}

TEST(FormatLog, JsonErrnoFields)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::error, "fail", "", "", 0, 13);
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::json);
    EXPECT_NE(out.find(R"("errno":13)"), std::string::npos);
    EXPECT_NE(out.find(R"("strerror":"Permission denied")"), std::string::npos);
}

TEST(FormatLog, JsonNanosecondPadding)
{
    using namespace std::chrono;
    const auto tp = system_clock::time_point{ milliseconds{ 5 } };
    const auto ctx = make_ctx_with_time(tp, linyaps_box::log::level::info, "x");
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::json);
    EXPECT_NE(out.find(".005000000Z"), std::string::npos)
      << "sub-second nanoseconds must be zero-padded to 9 digits, got: " << out;
}

#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
TEST(FormatLog, JsonSourceLocationFields)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::debug, "d", "a.cpp", "b", 3);
    const auto out = format_to_string(ctx, linyaps_box::log::output_format::json);
    EXPECT_NE(out.find(R"("file":"a.cpp")"), std::string::npos);
    EXPECT_NE(out.find(R"("line":3)"), std::string::npos);
    EXPECT_NE(out.find(R"("function":"b")"), std::string::npos);
}
#endif

} // namespace
