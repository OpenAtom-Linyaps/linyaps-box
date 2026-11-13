// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep
#include "linyaps_box/log/formatter.h"
#include "linyaps_box/log/logger.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/log/sinks/file_sink.h"
#include "linyaps_box/log/sinks/stderr_sink.h"
#include "linyaps_box/log/sinks/syslog_sink.h"
#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION
#  include "linyaps_box/log/sinks/journald_sink.h"
#endif
#include "linyaps_box/log/sink_factory.h"
#include "linyaps_box/os/result.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>

namespace {

using nlohmann_json = nlohmann::json;

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
    ctx.message = msg;
    ctx.time = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
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
            const auto n = ::read(pipe_fd[0], buf.data(), buf.size());
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

struct recording_sink : linyaps_box::log::sink
{
    struct record
    {
        linyaps_box::log::level lvl{ linyaps_box::log::level::error };
        std::string message;

        record() = default;

        record(linyaps_box::log::level l, std::string m)
            : lvl(l)
            , message(std::move(m))
        {
        }

#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        std::string file;
        std::string function;
        int line{ 0 };
#endif
    };

    mutable std::vector<record> records;

    auto log([[maybe_unused]] fmt::memory_buffer &buf,
             const linyaps_box::log::log_context &ctx) const noexcept -> void override
    {
        record entry{ };
        entry.lvl = ctx.lvl;
        entry.message = ctx.message;
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
        entry.file = ctx.file;
        entry.function = ctx.function;
        entry.line = ctx.line;
#endif
        records.push_back(std::move(entry));
    }
};

struct mock_syslog_backend
{
    struct call
    {
        int priority;
        std::string msg;
    };

    mutable std::vector<call> calls;

    explicit mock_syslog_backend(std::string) { } // NOLINT

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
        for (auto const i : iov) {
            c.fields.emplace_back(static_cast<const char *>(i.iov_base), i.iov_len);
        }

        calls.push_back(std::move(c));
    }
};

using mock_journald_sink = linyaps_box::log::basic_journald_sink<mock_journald_backend>;

struct mock_journald_spec
{
    std::string ident;
};

auto has_field(const std::vector<std::string> &fields, std::string_view expected) -> bool
{
    return std::find(fields.begin(), fields.end(), expected) != fields.end();
}

#endif

class LogFixture : public ::testing::Test
{
protected:
    linyaps_box::log::level saved_level_;

    void SetUp() override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        saved_level_ = logger.get_level();
        logger.unset_backend();
    }

    void TearDown() override
    {
        auto &logger = linyaps_box::log::global_logger::instance();
        logger.unset_backend();
        logger.set_level(saved_level_);
    }
};

class LogIOSmoke : public LogFixture
{
};

TEST(FormatLog, JsonFields)
{
    const auto tp = std::chrono::system_clock::time_point{ std::chrono::milliseconds{ 5 } };
    auto ctx =
      make_ctx_with_time(tp, linyaps_box::log::level::error, "json test 42", "a.cpp", "b", 3);
    ctx.pid = 4242;

    const auto out = format_to_string(ctx, linyaps_box::log::output_format::json);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.back(), '\n');
    EXPECT_EQ(out.find('\n'), out.size() - 1) << "expected exactly one JSON line";

    const auto entry = nlohmann_json::parse(out);
    ASSERT_TRUE(entry.at("level").is_string());
    EXPECT_EQ(entry.at("level").get<std::string>(), "ERROR");
    ASSERT_TRUE(entry.at("message").is_string());
    EXPECT_EQ(entry.at("message").get<std::string>(), "json test 42");
    ASSERT_TRUE(entry.at("pid").is_number_integer());
    EXPECT_EQ(entry.at("pid").get<int>(), 4242);
    ASSERT_TRUE(entry.at("time").is_string());
    EXPECT_EQ(entry.at("time").get<std::string>(), "1970-01-01T00:00:00.005000000Z");
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    ASSERT_TRUE(entry.at("file").is_string());
    EXPECT_EQ(entry.at("file").get<std::string>(), "a.cpp");
    ASSERT_TRUE(entry.at("line").is_number_integer());
    EXPECT_EQ(entry.at("line").get<int>(), 3);
    ASSERT_TRUE(entry.at("function").is_string());
    EXPECT_EQ(entry.at("function").get<std::string>(), "b");
#endif

    EXPECT_FALSE(entry.contains("error_code"));
    EXPECT_FALSE(entry.contains("error_message"));
}

TEST(FormatLog, TextCarriesErrnoSuffix)
{
    auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::error, "operation failed");
    ctx.errno_ = 13;

    const auto out = format_to_string(ctx, linyaps_box::log::output_format::text);
    EXPECT_NE(out.find("operation failed"), std::string::npos);
    EXPECT_NE(out.find(linyaps_box::os::make_error_code(13).message()), std::string::npos);
}

TEST(FormatLog, TextMessageEdges)
{
    const auto text_of = [](std::string_view message) {
        return format_to_string(make_ctx_with_time({ }, linyaps_box::log::level::info, message),
                                linyaps_box::log::output_format::text);
    };

    const auto message_part = [](std::string_view out) {
        const auto pos = out.find("]:\n");
        EXPECT_NE(pos, std::string_view::npos) << out;
        return std::string{ out.substr(pos + 3) };
    };

    EXPECT_EQ(message_part(text_of("")), "    \n");
    EXPECT_EQ(message_part(text_of("a")), "    a\n");
    EXPECT_EQ(message_part(text_of("a\nb\nc")), "    a\n    b\n    c\n");
    EXPECT_EQ(message_part(text_of("a\n\nb")), "    a\n    \n    b\n");

    // Trailing newlines are trimmed.
    EXPECT_EQ(message_part(text_of("a\n\n")), "    a\n");
}

struct JsonMessageCase
{
    const char *name;
    std::string message;
    std::string expected;
};

class JsonMessageTest : public ::testing::TestWithParam<JsonMessageCase>
{
};

TEST_P(JsonMessageTest, RoundTripsThroughJson)
{
    const auto ctx = make_ctx_with_time({ }, linyaps_box::log::level::info, GetParam().message);
    const auto entry =
      nlohmann_json::parse(format_to_string(ctx, linyaps_box::log::output_format::json));
    EXPECT_EQ(entry.at("message").get<std::string>(), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
  All,
  JsonMessageTest,
  ::testing::Values(
    JsonMessageCase{ "quotes", R"(say "hi")", R"(say "hi")" },
    JsonMessageCase{ "backslash", R"(a\b)", R"(a\b)" },
    JsonMessageCase{ "newline", "l1\nl2", "l1\nl2" },
    JsonMessageCase{ "tab", "a\tb", "a\tb" },
    JsonMessageCase{ "nul", std::string{ "a" } + '\0' + "b", std::string{ "a" } + '\0' + "b" },
    JsonMessageCase{
      "non-ascii", "h\xC3\xA9llo \xF0\x9F\x98\x80", "h\xC3\xA9llo \xF0\x9F\x98\x80" },
    JsonMessageCase{ "invalid-utf8",
                     std::string{ "x" } + '\x80' + "y",
                     std::string{ "x" } + "\xEF\xBF\xBD" + "y" }));

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

class LevelFilterTest : public LogFixture, public ::testing::WithParamInterface<FilterCase>
{
};

TEST_P(LevelFilterTest, Filtering)
{
    auto &logger = linyaps_box::log::global_logger::instance();

    auto sink = std::make_unique<recording_sink>();
    auto *records = &sink->records;
    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.push_back(std::move(sink));
    logger.set_sinks(std::move(sinks));
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

    ASSERT_EQ(records->size(), GetParam().expect_output ? 1U : 0U);
    if (!records->empty()) {
        EXPECT_EQ((*records)[0].lvl, GetParam().msg_lvl);
        EXPECT_EQ((*records)[0].message, "test");
    }
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

struct DispatchCase
{
    linyaps_box::log::level msg;
    linyaps_box::log::level threshold;
    bool delivered;
};

class DispatchFilterTest : public LogFixture, public ::testing::WithParamInterface<DispatchCase>
{
};

TEST_P(DispatchFilterTest, LogAndContextShareTheThreshold)
{
    auto &logger = linyaps_box::log::global_logger::instance();

    auto sink = std::make_unique<recording_sink>();
    auto *records = &sink->records;
    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.push_back(std::move(sink));
    logger.set_sinks(std::move(sinks));
    logger.set_level(GetParam().threshold);

    logger.dispatch_log(GetParam().msg, "via-log", { }, "test.cpp", "fn", 1);
    logger.dispatch_context(make_ctx(GetParam().msg, "via-context"));

    ASSERT_EQ(records->size(), GetParam().delivered ? 2U : 0U);
    if (GetParam().delivered) {
        EXPECT_EQ((*records)[0].message, "via-log");
        EXPECT_EQ((*records)[1].message, "via-context");
    }
}

INSTANTIATE_TEST_SUITE_P(
  All,
  DispatchFilterTest,
  ::testing::Values(
    DispatchCase{ linyaps_box::log::level::debug, linyaps_box::log::level::info, false },
    DispatchCase{ linyaps_box::log::level::info, linyaps_box::log::level::info, true },
    DispatchCase{ linyaps_box::log::level::warn, linyaps_box::log::level::info, true },
    DispatchCase{ linyaps_box::log::level::error, linyaps_box::log::level::info, true },
    DispatchCase{ linyaps_box::log::level::warn, linyaps_box::log::level::error, false },
    DispatchCase{ linyaps_box::log::level::error, linyaps_box::log::level::error, true },
    DispatchCase{ linyaps_box::log::level::debug, linyaps_box::log::level::debug, true }));

TEST_F(LogFixture, DispatchReachesEverySink)
{
    auto &logger = linyaps_box::log::global_logger::instance();

    auto first = std::make_unique<recording_sink>();
    auto second = std::make_unique<recording_sink>();
    auto *first_records = &first->records;
    auto *second_records = &second->records;
    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.push_back(std::move(first));
    sinks.push_back(std::move(second));
    logger.set_sinks(std::move(sinks));
    logger.set_level(linyaps_box::log::level::info);

    logger.dispatch_context(make_ctx(linyaps_box::log::level::error, "fanout"));

    ASSERT_EQ(first_records->size(), 1U);
    ASSERT_EQ(second_records->size(), 1U);
    EXPECT_EQ((*first_records)[0].message, "fanout");
    EXPECT_EQ((*second_records)[0].message, "fanout");
}

TEST_F(LogFixture, MacrosDeliverWhenCompiledAndAllowed)
{
    auto &logger = linyaps_box::log::global_logger::instance();

    auto sink = std::make_unique<recording_sink>();
    auto *records = &sink->records;
    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.push_back(std::move(sink));
    logger.set_sinks(std::move(sinks));
    logger.set_level(linyaps_box::log::level::debug);

    LINYAPS_BOX_LOG_FATAL("fatal");
    LINYAPS_BOX_LOG_ERROR("error");
    LINYAPS_BOX_LOG_WARN("warn");
    LINYAPS_BOX_LOG_INFO("info");
    LINYAPS_BOX_LOG_DEBUG("debug");

    // FATAL and ERROR are always compiled in; WARN/INFO/DEBUG depend on the
    // level this translation unit was built with.
    std::vector<recording_sink::record> expected{
        { linyaps_box::log::level::fatal, "fatal" },
        { linyaps_box::log::level::error, "error" },
    };
    if (LINYAPS_BOX_LOG_ACTIVE_LEVEL >= static_cast<int>(linyaps_box::log::level::warn)) {
        expected.push_back({ linyaps_box::log::level::warn, "warn" });
    }
    if (LINYAPS_BOX_LOG_ACTIVE_LEVEL >= static_cast<int>(linyaps_box::log::level::info)) {
        expected.push_back({ linyaps_box::log::level::info, "info" });
    }
    if (LINYAPS_BOX_LOG_ACTIVE_LEVEL >= static_cast<int>(linyaps_box::log::level::debug)) {
        expected.push_back({ linyaps_box::log::level::debug, "debug" });
    }

    ASSERT_EQ(records->size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ((*records)[i].lvl, expected[i].lvl);
        EXPECT_EQ((*records)[i].message, expected[i].message);
    }
}

// Real descriptor I/O: a few transport checks, kept apart from the unit tests
// above so they can be selected (or skipped) as a group.
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
TEST_F(LogFixture, MacrosCarrySourceLocation)
{
    auto &logger = linyaps_box::log::global_logger::instance();

    auto sink = std::make_unique<recording_sink>();
    auto *records = &sink->records;
    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.push_back(std::move(sink));
    logger.set_sinks(std::move(sinks));
    logger.set_level(linyaps_box::log::level::debug);

    const int expected_line = __LINE__ + 1;
    LINYAPS_BOX_LOG_ERROR("loc");

    ASSERT_EQ(records->size(), 1U);
    EXPECT_EQ((*records)[0].file, "log_test.cpp");
    EXPECT_EQ((*records)[0].function, __func__);
    EXPECT_EQ((*records)[0].line, expected_line);
}
#endif

TEST(SyslogSink, FormatMatrix)
{
    struct Case
    {
        linyaps_box::log::output_format fmt;
        bool cee;
        bool json;
    };

    const std::array<Case, 4> cases{
        Case{ linyaps_box::log::output_format::text, false, false },
        Case{ linyaps_box::log::output_format::text, true, false },
        Case{ linyaps_box::log::output_format::json, false, true },
        Case{ linyaps_box::log::output_format::json, true, true },
    };

    for (const auto &c : cases) {
        SCOPED_TRACE(static_cast<int>(c.fmt) * 2 + static_cast<int>(c.cee));

        const mock_syslog_sink sink(mock_syslog_spec{ "test_app", c.cee }, c.fmt);
        fmt::memory_buffer buf;
        sink.log(buf, make_ctx(linyaps_box::log::level::warn, "matrix"));

        ASSERT_EQ(sink.backend().calls.size(), 1U);
        const auto &call = sink.backend().calls[0];
        EXPECT_EQ(call.priority, LOG_WARNING);

        const bool has_cee = call.msg.rfind("@cee: ", 0) == 0;
        EXPECT_EQ(has_cee, c.cee && c.json);

        const auto payload = has_cee ? call.msg.substr(6) : call.msg;
        if (c.json) {
            const auto entry = nlohmann_json::parse(payload);
            EXPECT_EQ(entry.at("message").get<std::string>(), "matrix");
            EXPECT_EQ(entry.at("level").get<std::string>(), "WARN");
        } else {
            EXPECT_NE(payload.find("matrix"), std::string::npos);
            EXPECT_NE(payload.find("[WARN ]"), std::string::npos);
        }
    }
}

TEST(SyslogSink, HandlesLargeMessage)
{
    const mock_syslog_sink sink(mock_syslog_spec{ "test_app", false },
                                linyaps_box::log::output_format::json);
    const std::string message(1U << 20, 'm');

    fmt::memory_buffer buf;
    sink.log(buf, make_ctx(linyaps_box::log::level::info, message));

    ASSERT_EQ(sink.backend().calls.size(), 1U);
    const auto entry = nlohmann_json::parse(sink.backend().calls[0].msg);
    EXPECT_EQ(entry.at("message").get<std::string>(), message);
}

#ifdef LINYAPS_BOX_ENABLE_SYSTEMD_INTEGRATION

TEST(JournaldSink, PassesAllFields)
{
    mock_journald_backend::calls.clear();
    const mock_journald_sink sink(mock_journald_spec{ "myapp" },
                                  linyaps_box::log::output_format::text);
    auto ctx = make_ctx(linyaps_box::log::level::fatal, "segfault", "crash.c", "deref", 77);
    ctx.errno_ = 13;
    fmt::memory_buffer buf;
    sink.log(buf, ctx);

    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    auto fields = mock_journald_backend::calls[0].fields;
    std::vector<std::string> expected{
        "MESSAGE=segfault",
        "PRIORITY=2",
        "SYSLOG_IDENTIFIER=myapp",
        "ERRNO=13",
    };
#  ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    expected.emplace_back("CODE_FILE=crash.c");
    expected.emplace_back("CODE_LINE=77");
    expected.emplace_back("CODE_FUNC=deref");
#  endif
    std::sort(fields.begin(), fields.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(fields, expected);
}

TEST(JournaldSink, ReplacesInvalidUtf8InMessage)
{
    mock_journald_backend::calls.clear();
    const mock_journald_sink sink(mock_journald_spec{ "myapp" },
                                  linyaps_box::log::output_format::text);

    const std::string bad = std::string{ "x" } + '\x80' + "y";
    fmt::memory_buffer buf;
    sink.log(buf, make_ctx(linyaps_box::log::level::error, bad));

    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    EXPECT_TRUE(has_field(mock_journald_backend::calls[0].fields, "MESSAGE=x\xEF\xBF\xBDy"));
}

TEST(JournaldSink, PassesValidUtf8ThroughUnchanged)
{
    mock_journald_backend::calls.clear();
    const mock_journald_sink sink(mock_journald_spec{ "myapp" },
                                  linyaps_box::log::output_format::text);

    const std::string message = "h\xC3\xA9llo \xF0\x9F\x98\x80 world";
    fmt::memory_buffer buf;
    sink.log(buf, make_ctx(linyaps_box::log::level::info, message));

    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    EXPECT_TRUE(has_field(mock_journald_backend::calls[0].fields, "MESSAGE=" + message));
}

TEST(JournaldSink, SanitizesEachFieldIndependently)
{
    mock_journald_backend::calls.clear();
    const std::string ident = std::string{ "i" } + '\x80' + "d";
    const mock_journald_sink sink(mock_journald_spec{ ident },
                                  linyaps_box::log::output_format::text);

    const std::string message = std::string{ "a" } + '\0' + "b";
    fmt::memory_buffer buf;
    sink.log(buf, make_ctx(linyaps_box::log::level::error, message));

    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    const auto &fields = mock_journald_backend::calls[0].fields;
    EXPECT_TRUE(has_field(fields, "MESSAGE=a\\u0000b"));
    EXPECT_TRUE(has_field(fields,
                          "SYSLOG_IDENTIFIER=i\xEF\xBF\xBD"
                          "d"));
}

TEST(JournaldSink, PreservesLargeFieldContents)
{
    mock_journald_backend::calls.clear();

    fmt::memory_buffer buf;
    buf.reserve(1024);

    const std::string ident(buf.capacity(), 'i');
    const std::string message(buf.capacity() / 2, 'm');
    const mock_journald_sink sink(mock_journald_spec{ ident },
                                  linyaps_box::log::output_format::text);

    sink.log(buf, make_ctx(linyaps_box::log::level::error, message));

    // Only the two large fields matter here: they are what a stale pointer
    // would corrupt.  The full field set is checked by PassesAllFields.
    ASSERT_EQ(mock_journald_backend::calls.size(), 1U);
    const auto &fields = mock_journald_backend::calls[0].fields;
    EXPECT_TRUE(has_field(fields, "MESSAGE=" + message));
    EXPECT_TRUE(has_field(fields, "SYSLOG_IDENTIFIER=" + ident));
}

#endif

TEST(LogConfig, MakeSinkRejectsUnknownScheme)
{
    EXPECT_THROW(
      std::ignore =
        linyaps_box::log::make_sink("unknown:xxx", linyaps_box::log::output_format::text, false),
      std::runtime_error);
}

TEST(LoggerDeathTest, NoBackendTerminates)
{
    auto &logger = linyaps_box::log::global_logger::instance();
    const auto saved_level = logger.get_level();
    logger.unset_backend();
    logger.set_level(linyaps_box::log::level::debug);

    // With no sink there is nowhere to report, and dispatch is noexcept, so the
    // process must not continue.  The diagnostic text is not a contract.
    EXPECT_DEATH(LINYAPS_BOX_LOG_ERROR("should crash"), ".*");

    logger.set_level(saved_level);
}

TEST_F(LogIOSmoke, FileText)
{
    std::string tmp{ "/tmp/ll_box_test_XXXXXX" };
    const auto ret = ::mkstemp(const_cast<char *>(tmp.data()));
    ASSERT_GE(ret, 0);
    linyaps_box::utils::file_descriptor fd{ ret };

    auto &logger = linyaps_box::log::global_logger::instance();
    {
        std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
        sinks.push_back(std::make_unique<linyaps_box::log::file_sink>(
          linyaps_box::log::file_spec{ std::move(fd) },
          linyaps_box::log::output_format::text));
        logger.set_sinks(std::move(sinks));
    }
    logger.set_level(linyaps_box::log::level::debug);

    LINYAPS_BOX_LOG_ERROR("file test {}", 99);

    std::ifstream in(tmp);
    std::string line;
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_NE(line.find("[ERROR]"), std::string::npos);
    ASSERT_TRUE(std::getline(in, line));
    EXPECT_NE(line.find("    file test 99"), std::string::npos);

    std::filesystem::remove(tmp);
}

TEST_F(LogIOSmoke, ContextDispatch)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    {
        std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
        sinks.push_back(
          std::make_unique<linyaps_box::log::stderr_sink>(linyaps_box::log::stderr_spec{ },
                                                          linyaps_box::log::output_format::text));
        logger.set_sinks(std::move(sinks));
    }
    logger.set_level(linyaps_box::log::level::error);

    const auto ctx = make_ctx(linyaps_box::log::level::error, "raw dispatch", "test.cpp", "fn", 42);
    logger.dispatch_context(ctx);

    const auto output = cap.extract();
    EXPECT_NE(output.find("raw dispatch"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_NE(output.find("test.cpp:42"), std::string::npos);
#endif
}

TEST_F(LogIOSmoke, MultiSink)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();

    std::string tmp{ "/tmp/ll_box_test_XXXXXX" };
    const auto ret = ::mkstemp(const_cast<char *>(tmp.data()));
    ASSERT_GE(ret, 0);
    linyaps_box::utils::file_descriptor fd{ ret };

    std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
    sinks.emplace_back(
      std::make_unique<linyaps_box::log::stderr_sink>(linyaps_box::log::stderr_spec{ },
                                                      linyaps_box::log::output_format::text));
    sinks.emplace_back(
      std::make_unique<linyaps_box::log::file_sink>(linyaps_box::log::file_spec{ std::move(fd) },
                                                    linyaps_box::log::output_format::text));
    logger.set_sinks(std::move(sinks));
    logger.set_level(linyaps_box::log::level::info);

    LINYAPS_BOX_LOG_ERROR("multi sink test {}", 42);

    const auto output = cap.extract();
    EXPECT_NE(output.find("multi sink test 42"), std::string::npos);
    EXPECT_NE(output.find("[ERROR]"), std::string::npos);
#ifdef LINYAPS_BOX_LOG_ENABLE_SOURCE_LOCATION
    EXPECT_NE(output.find("log_test.cpp"), std::string::npos);
#endif

    std::ifstream in(tmp);
    const std::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("multi sink test 42"), std::string::npos);
    EXPECT_NE(content.find("[ERROR]"), std::string::npos);

    std::filesystem::remove(tmp);
    logger.unset_backend();
}

TEST_F(LogIOSmoke, StderrJsonCarriesErrno)
{
    stderr_capture cap;
    auto &logger = linyaps_box::log::global_logger::instance();
    {
        std::vector<std::unique_ptr<linyaps_box::log::sink>> sinks;
        sinks.push_back(
          std::make_unique<linyaps_box::log::stderr_sink>(linyaps_box::log::stderr_spec{ },
                                                          linyaps_box::log::output_format::json));
        logger.set_sinks(std::move(sinks));
    }
    logger.set_level(linyaps_box::log::level::error);

    LINYAPS_BOX_LOG_ERROR_ERRNO(EACCES, "operation failed");

    const auto output = cap.extract();
    ASSERT_FALSE(output.empty());
    EXPECT_EQ(output.back(), '\n');

    // Macro -> dispatcher -> JSON formatter: errno has to survive the chain.
    const auto entry = nlohmann_json::parse(output);
    EXPECT_EQ(entry.at("level").get<std::string>(), "ERROR");
    ASSERT_TRUE(entry.at("message").is_string());
    EXPECT_EQ(entry.at("message").get<std::string>(), "operation failed");
    ASSERT_TRUE(entry.at("error_code").is_number_integer());
    EXPECT_EQ(entry.at("error_code").get<int>(), EACCES);
    ASSERT_TRUE(entry.at("error_message").is_string());
    EXPECT_EQ(entry.at("error_message").get<std::string>(),
              linyaps_box::os::make_error_code(EACCES).message());
}

TEST_F(LogIOSmoke, MakeSinkConstructsRealBackends)
{
    using linyaps_box::log::output_format;

    EXPECT_NO_THROW(std::ignore =
                      linyaps_box::log::make_sink("stderr", output_format::text, false));
    EXPECT_NO_THROW(std::ignore =
                      linyaps_box::log::make_sink("syslog:test_ident", output_format::json, true));
}

} // namespace
