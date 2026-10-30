// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "gtest_compat.h" // IWYU pragma: keep
#include "linyaps_box/io/stream.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/io.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <unistd.h>

namespace io = linyaps_box::io;
namespace os = linyaps_box::os;
namespace utils = linyaps_box::utils;

namespace {

// A temp directory removed on destruction, so a failing assertion cannot leak it.
class temp_dir
{
public:
    temp_dir()
    {
        auto tmpl = std::filesystem::temp_directory_path().string();
        tmpl.append("/linyaps-box-stream-test-XXXXXX");

        const auto *created = ::mkdtemp(tmpl.data());
        if (created == nullptr) {
            throw std::system_error{ errno, std::generic_category(), "mkdtemp" };
        }

        path_ = created;
    }

    ~temp_dir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    temp_dir(const temp_dir &) = delete;
    temp_dir(temp_dir &&) = delete;
    temp_dir &operator=(const temp_dir &) = delete;
    temp_dir &operator=(temp_dir &&) = delete;

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return path_; }

    [[nodiscard]] auto write(const std::filesystem::path &name, std::string_view content) const
      -> std::filesystem::path
    {
        const auto file = path_ / name;
        std::ofstream out{ file, std::ios::binary | std::ios::trunc };
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return file;
    }

private:
    std::filesystem::path path_;
};

auto open_for_reading(const std::filesystem::path &path) -> os::Result<utils::file_descriptor>
{
    return os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only });
}

// The path overload takes its size hint from st_size, so this is the single-read path.
TEST(StreamReadAll, ReadsRegularFile)
{
    const temp_dir dir;
    const auto file = dir.write("config.json", R"({"ociVersion":"1.3.0"})");

    EXPECT_EQ(os::throw_if_error(io::read_all(file)), R"({"ociVersion":"1.3.0"})");
}

// The bytes are returned verbatim: a NUL is content here, not a terminator.
TEST(StreamReadAll, KeepsEmbeddedNulBytes)
{
    const temp_dir dir;
    const std::string content{ "a\0b", 3 };
    const auto file = dir.write("binary", content);

    const auto read = os::throw_if_error(io::read_all(file));
    EXPECT_EQ(read.size(), 3U);
    EXPECT_EQ(read, content);
}

TEST(StreamReadAll, EmptyFileIsEmpty)
{
    const temp_dir dir;
    const auto file = dir.write("empty", "");

    EXPECT_TRUE(os::throw_if_error(io::read_all(file)).empty());
}

// A hint smaller than the content forces the growth path; the read must still
// return everything.
TEST(StreamReadAll, GrowsPastASmallHint)
{
    const temp_dir dir;
    const std::string content(5000, 'x');
    const auto file = dir.write("big", content);

    auto fd = os::throw_if_error(open_for_reading(file));
    EXPECT_EQ(os::throw_if_error(io::read_all(fd, 1)), content);
}

TEST(StreamReadAll, ShrinksAfterAnOverlargeHint)
{
    const temp_dir dir;
    const auto file = dir.write("small", "hello");

    auto fd = os::throw_if_error(open_for_reading(file));
    EXPECT_EQ(os::throw_if_error(io::read_all(fd, 64 * 1024)), "hello");
}

// /proc files report st_size 0 and fall through to the default read size.
TEST(StreamReadAll, ReadsAFileWithoutASizeHint)
{
    const auto stat = os::throw_if_error(io::read_all(std::filesystem::path{ "/proc/self/stat" }));

    EXPECT_FALSE(stat.empty());
    EXPECT_NE(stat.find('('), std::string::npos) << "expected the comm field";
}

// The fd overload starts at the descriptor's current offset.
TEST(StreamReadAll, ReadsFromTheCurrentOffset)
{
    const temp_dir dir;
    const auto file = dir.write("offset", "0123456789");

    auto fd = os::throw_if_error(open_for_reading(file));
    std::array<std::byte, 4> head{ };
    EXPECT_EQ(os::throw_if_error(os::read(fd, head)), head.size());

    EXPECT_EQ(os::throw_if_error(io::read_all(fd)), "456789");
}

// A directory opens read-only, so the regular-file check is what rejects it.
TEST(StreamReadAll, RejectsNonRegularFile)
{
    const temp_dir dir;

    const auto content = io::read_all(dir.path());
    ASSERT_FALSE(content);
    EXPECT_EQ(content.error(), std::make_error_code(std::errc::invalid_argument));
}

TEST(StreamReadAll, ReportsMissingFile)
{
    const temp_dir dir;

    const auto content = io::read_all(dir.path() / "missing");
    ASSERT_FALSE(content);
    EXPECT_EQ(content.error(), std::make_error_code(std::errc::no_such_file_or_directory));
}

} // namespace
