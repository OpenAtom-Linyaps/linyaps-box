// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// Skipped tests (require root privileges or kernel features not available
// in non-root CI environments):
//   - MS_NOSYMFOLLOW mount tests (requires mount with MS_NOSYMFOLLOW)
//   - fs.protected_symlinks tests (requires chown to change file ownership)
//   - Character/block device mknod tests (requires CAP_MKNOD)
//   - RENAME_WHITEOUT tests (requires kernel support and CAP_MKNOD)
//   - setgid directory tests (requires chown to change directory ownership)
//   - Multithreaded race tests (require RENAME_EXCHANGE racing with resolution)
//   - fifo/socket removal tests (fifo/socket creation requires mknod)

#include "gtest_compat.h"
#include "linyaps_box/infra/rootfs.h"
#include "linyaps_box/os/fs.h"

#include <fmt/std.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace os = linyaps_box::os;
namespace infra = linyaps_box::infra;
namespace utils = linyaps_box::utils;

namespace {

constexpr auto mkdirs_perm = std::filesystem::perms::owner_all | std::filesystem::perms::group_exec
  | std::filesystem::perms::others_exec;
constexpr auto tmpfile_perm =
  std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;
constexpr auto invalidmode_perm = std::filesystem::perms::set_gid
  | std::filesystem::perms::owner_all | std::filesystem::perms::group_all
  | std::filesystem::perms::others_all;

class TempDir
{
public:
    TempDir(const TempDir &) = delete;
    TempDir(TempDir &&) noexcept = default;
    TempDir &operator=(const TempDir &) = delete;
    TempDir &operator=(TempDir &&) noexcept = default;

    TempDir() noexcept
    {
        auto tmpl = std::filesystem::temp_directory_path().string();
        tmpl.append("/linyaps-box-vfs-test-XXXXXX");

        auto *p = ::mkdtemp(tmpl.data());
        if (p == nullptr) {
            fmt::println(std::cerr,
                         "failed to create test dir: {}",
                         linyaps_box::os::make_error_code(errno));
        }
        path_ = p;
    }

    ~TempDir() noexcept
    {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            if (ec) {
                fmt::println(std::cerr,
                             "failed to remove test dir {} :{}, remove it manually",
                             path_,
                             ec);
            }
        }
    }

    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return path_; }

private:
    std::filesystem::path path_;
};

auto write_file(const std::filesystem::path &p, std::string_view contents) -> void
{
    std::ofstream f(p);
    f << contents;
}

struct BasicTree
{
    TempDir root;
    std::filesystem::path base;
    std::error_code ec;

    BasicTree()
    {
        base = root.path() / "base";
        std::filesystem::create_directories(base);

        std::filesystem::create_directories(base / "a");
        std::filesystem::create_directories(base / "b/c/d/e/f");
        std::filesystem::create_directories(base / "target");
        std::filesystem::create_directories(base / "link1");
        std::filesystem::create_directories(base / "link2");
        std::filesystem::create_directories(base / "link3");
        std::filesystem::create_directories(base / "loop");
        std::filesystem::create_directories(base / "deep-rmdir" / "a" / "b" / "c");
        std::filesystem::create_directories(base / "deep-rmdir" / "x" / "y" / "z");

        // Fifo and socket (removed from BasicTree_node for non-root)
        // Skipped: fifo/sock require mknod which may not be available

        write_file(base / "b/c/file", "file content");

        std::filesystem::create_symlink("/b/c/d/e", base / "e");
        std::filesystem::create_symlink("b/c/file", base / "b-file");
        std::filesystem::create_symlink("/", base / "root-link1");
        std::filesystem::create_symlink("/..", base / "root-link2");
        std::filesystem::create_symlink("a/fake", base / "a-fake1");
        std::filesystem::create_symlink("a/fake/foo/bar/..", base / "a-fake2");
        std::filesystem::create_symlink("a/fake/../../b", base / "a-fake3");
        std::filesystem::create_symlink("/target", base / "link1" / "target_abs");
        std::filesystem::create_symlink("../target", base / "link1" / "target_rel");
        std::filesystem::create_symlink("/link1", base / "link2" / "link1_abs");
        std::filesystem::create_symlink("../link1", base / "link2" / "link1_rel");
        std::filesystem::create_symlink("/link2/link1_rel/target_rel",
                                        base / "link3" / "target_abs");
        std::filesystem::create_symlink("../link2/link1_rel/target_rel",
                                        base / "link3" / "target_rel");

        std::filesystem::create_symlink("a", base / "loop" / "link");
        std::filesystem::create_symlink("basic-loop1", base / "loop" / "basic-loop1");
        std::filesystem::create_symlink("/loop/basic-loop2", base / "loop" / "basic-loop2");
        std::filesystem::create_symlink("../loop/basic-loop3", base / "loop" / "basic-loop3");
    }

    [[nodiscard]] auto open() const -> infra::Root { return infra::Root::open(base).value(); }
};

// Returns the operation's error message, or an empty string on success. Used by
// the parameterized operation tables where only the outcome matters.
template <typename T>
auto result_message(const os::Result<T> &r) -> std::string
{
    return r.has_value() ? std::string{ } : r.error().message();
}

struct SymlinkedDirCase
{
    const char *name;
    std::function<void(const TempDir &)> prepare;
    std::function<std::string(infra::Root &)> op;
    std::function<void(const TempDir &)> check;
};
LINYAPS_BOX_NAMED_PRINTER(SymlinkedDirCase)

class VfsScopedThroughSymlinkedDir : public ::testing::TestWithParam<SymlinkedDirCase>
{
};

TEST_P(VfsScopedThroughSymlinkedDir, ThroughSymlinkedDir)
{
    const auto &c = GetParam();

    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();

    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();

    if (c.prepare) {
        c.prepare(d);
    }

    ASSERT_EQ(c.op(*root), "") << "operation through symlinked dir failed";
    c.check(d);
}

auto get_symlinked_dir_testcases() noexcept // NOLINT(readability-function-cognitive-complexity)
  -> const std::vector<SymlinkedDirCase> &
{
    const static std::vector<SymlinkedDirCase> symlinked_dir_cases{
        {
          "MkdirAllThroughSymlinkedDir",
          nullptr,
          [](infra::Root &root) {
              return result_message(root.create_directories("bin/foo", os::default_dir_perm));
          },
          [](const TempDir &d) {
              EXPECT_TRUE(std::filesystem::is_directory(d.path() / "real" / "foo"));
          },
        },
        {
          "CreateFileThroughSymlinkedDir",
          nullptr,
          [](infra::Root &root) {
              return result_message(root.create_file("bin/newfile",
                                                     os::sys::open_flag::none,
                                                     os::default_new_file_perm));
          },
          [](const TempDir &d) {
              EXPECT_TRUE(std::filesystem::is_regular_file(d.path() / "real" / "newfile"));
          },
        },
        {
          "CreateDirectoryThroughSymlinkedDir",
          nullptr,
          [](infra::Root &root) {
              return result_message(root.create_directory("bin/newdir", os::default_dir_perm));
          },
          [](const TempDir &d) {
              EXPECT_TRUE(std::filesystem::is_directory(d.path() / "real" / "newdir"));
          },
        },
        {
          "CreateSymlinkThroughSymlinkedDir",
          nullptr,
          [](infra::Root &root) {
              return result_message(root.create("bin/mylink", infra::symlink_spec{ "/target" }));
          },
          [](const TempDir &d) {
              EXPECT_TRUE(std::filesystem::is_symlink(d.path() / "real" / "mylink"));
          },
        },
        {
          "RemoveFileThroughSymlinkedDir",
          [](const TempDir &d) {
              write_file(d.path() / "real" / "target", "data");
          },
          [](infra::Root &root) {
              return result_message(root.remove_file("bin/target"));
          },
          [](const TempDir &d) {
              EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "target"));
          },
        },
        {
          "RemoveDirThroughSymlinkedDir",
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "real" / "subdir");
          },
          [](infra::Root &root) {
              return result_message(root.remove_dir("bin/subdir"));
          },
          [](const TempDir &d) {
              EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "subdir"));
          },
        },
        {
          "RemoveAllThroughSymlinkedDir",
          [](const TempDir &d) {
              const auto dir = d.path() / "real" / "target";
              std::filesystem::create_directories(dir);
              write_file(dir / "file", "data");
          },
          [](infra::Root &root) {
              return result_message(root.remove_all("bin/target"));
          },
          [](const TempDir &d) {
              EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "target"));
          },
        },
        {
          "RenameThroughSymlinkedDir",
          [](const TempDir &d) {
              write_file(d.path() / "real" / "src", "data");
          },
          [](infra::Root &root) {
              return result_message(root.rename("bin/src", "bin/dst"));
          },
          [](const TempDir &d) {
              EXPECT_TRUE(std::filesystem::exists(d.path() / "real" / "dst"));
              EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "src"));
          },
        },
    };

    return symlinked_dir_cases;
}

INSTANTIATE_TEST_SUITE_P(ScopedThroughSymlinkedDir,
                         VfsScopedThroughSymlinkedDir,
                         ::testing::ValuesIn(get_symlinked_dir_testcases()),
                         [](const ::testing::TestParamInfo<SymlinkedDirCase> &info) {
                             return std::string{ info.param.name };
                         });

struct ResolveCase
{
    const char *name;
    std::vector<const char *> paths;
    enum class Expect : uint8_t {
        directory,
        regular,
        symlink,
        content,
        failure,
        failure_error,
    } expect;

    utils::bitflags<os::sys::open_flag> flags{ os::sys::open_flag::none };
    os::sys::access_mode mode{ os::sys::access_mode::path };
    const char *content{ nullptr };
    std::error_code error{ };
};
LINYAPS_BOX_NAMED_PRINTER(ResolveCase)

class VfsResolve : public ::testing::TestWithParam<ResolveCase>
{
};

TEST_P(VfsResolve, Resolve)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    const auto &c = GetParam();

    for (const auto *path : c.paths) {
        SCOPED_TRACE(path);
        auto fd = root.open(path, os::sys::open_option{ c.flags, c.mode });
        switch (c.expect) {
        case ResolveCase::Expect::directory: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            auto st = os::fstat(fd->ref());
            ASSERT_TRUE(st);
            EXPECT_TRUE(S_ISDIR(st->st_mode)) << path;
        } break;
        case ResolveCase::Expect::regular: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            auto st = os::fstat(fd->ref());
            ASSERT_TRUE(st);
            EXPECT_TRUE(S_ISREG(st->st_mode)) << path;
        } break;
        case ResolveCase::Expect::symlink: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            auto st = os::fstat(fd->ref());
            ASSERT_TRUE(st);
            EXPECT_TRUE(S_ISLNK(st->st_mode)) << path;
        } break;
        case ResolveCase::Expect::content: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            std::array<char, 16> buf{ };
            const auto n = std::string_view{ c.content }.size();
            EXPECT_EQ(::read(fd->get(), buf.data(), n), static_cast<ssize_t>(n));
            EXPECT_EQ(std::string_view(buf.data(), n), c.content);
        } break;
        case ResolveCase::Expect::failure: {
            EXPECT_FALSE(fd) << path << " should be rejected";
        } break;
        case ResolveCase::Expect::failure_error: {
            ASSERT_FALSE(fd) << path;
            EXPECT_TRUE(fd.error() == c.error) << path << ": " << fd.error().message();
        } break;
        }
    }
}

auto get_resolve_testcases() noexcept -> const std::vector<ResolveCase> &
{
    const static std::vector<ResolveCase> resolve_cases{
        {
          "DirectoryPaths",
          {
            "/",
            "/../../../../..",
            "a",
            "b/c",
            "b/c/d/e/f",
            "b///././c////.//d/./././///e////.//./f//././././",
            "root-link1",
            "root-link2",
            "e",
            "link1/target_abs",
            "link1/target_rel",
            "link2/link1_abs/target_abs",
            "link2/link1_abs/target_rel",
            "link2/link1_rel/target_abs",
            "link2/link1_rel/target_rel",
            "link3/target_abs",
            "link3/target_rel",
          },
          ResolveCase::Expect::directory,
        },
        { "CompleteFile", { "b/c/file" }, ResolveCase::Expect::regular },
        {
          "SymlinkRelative",
          { "b-file" },
          ResolveCase::Expect::content,
          os::sys::open_flag::none,
          os::sys::access_mode::read_only,
          "file content",
        },
        {
          "NofollowSymlink",
          { "b-file" },
          ResolveCase::Expect::symlink,
          os::sys::open_flag::no_follow,
        },
        {
          "NofollowSymlinkDirectory",
          { "b-file" },
          ResolveCase::Expect::failure_error,
          os::sys::open_flag::no_follow | os::sys::open_flag::directory,
          os::sys::access_mode::path,
          nullptr,
          std::make_error_code(std::errc::not_a_directory),
        },
        {
          "NofollowSymlinkRead",
          { "b-file" },
          ResolveCase::Expect::failure_error,
          os::sys::open_flag::no_follow,
          os::sys::access_mode::read_only,
          nullptr,
          std::make_error_code(std::errc::too_many_symbolic_link_levels),
        },
        { "NonExistent", { "a/b/c/d/e/f/g/h" }, ResolveCase::Expect::failure },
        { "ComponentThroughFile", { "b/c/file/../foo" }, ResolveCase::Expect::failure },
        { "TrailingSlashFailures", { "b/c/file/", "b-file/" }, ResolveCase::Expect::failure },
        {
          "SymlinkLoop",
          { "loop/basic-loop1", "loop/basic-loop2", "loop/basic-loop3", "loop/link" },
          ResolveCase::Expect::failure,
        },
        {
          "DanglingSymlinks",
          {
            "a-fake1",
            "a-fake2",
            "a-fake3",
            "dangling-tricky/deep1",
            "dangling-tricky/deep2",
            "dangling/a",
            "dangling/b/c",
            "dangling/c",
            "dangling/d/e",
            "dangling/e",
            "dangling/g",
            "c/a-fake1",
            "c/a-fake2",
            "c/a-fake3",
          },
          ResolveCase::Expect::failure,
        },
        {
          "NonLexicalPaths",
          {
            "link1/target_abs/foo",
            "link1/target_rel/foo",
            "link2/link1_abs/target_abs/foo",
            "link2/link1_abs/target_rel/foo",
            "link2/link1_rel/target_abs/foo",
            "link2/link1_rel/target_rel/foo",
            "link3/target_abs/foo",
            "link3/target_rel/foo",
          },
          ResolveCase::Expect::failure,
        },
    };

    return resolve_cases;
}

INSTANTIATE_TEST_SUITE_P(ResolveCases,
                         VfsResolve,
                         ::testing::ValuesIn(get_resolve_testcases()),
                         [](const ::testing::TestParamInfo<ResolveCase> &info) {
                             return std::string{ info.param.name };
                         });

struct ReadlinkCase
{
    const char *name;
    std::vector<const char *> paths;
    enum class Expect : uint8_t {
        target,
        success,
        failure,
        failure_error,
        success_contains,
        failure_one_of,
    } expect;

    const char *target{ nullptr };
    const char *contains{ nullptr };
    std::error_code error{ };
    std::vector<std::error_code> errors{ }; // NOLINT
};
LINYAPS_BOX_NAMED_PRINTER(ReadlinkCase)

class VfsReadlink : public ::testing::TestWithParam<ReadlinkCase>
{
};

TEST_P(VfsReadlink, Readlink)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    const auto &c = GetParam();

    for (const auto *path : c.paths) {
        SCOPED_TRACE(path);

        auto target = root.readlink(path);
        switch (c.expect) {
        case ReadlinkCase::Expect::target: {
            ASSERT_TRUE(target) << path << ": " << target.error().message();
            EXPECT_EQ(*target, c.target) << path;
        } break;
        case ReadlinkCase::Expect::success: {
            ASSERT_TRUE(target) << path << ": " << target.error().message();
        } break;
        case ReadlinkCase::Expect::failure: {
            EXPECT_FALSE(target) << path;
        } break;
        case ReadlinkCase::Expect::failure_error: {
            ASSERT_FALSE(target) << path;
            EXPECT_TRUE(target.error() == c.error) << path << ": " << target.error().message();
        } break;
        case ReadlinkCase::Expect::success_contains: {
            if (target) {
                EXPECT_NE(target->native().find(c.contains), std::string::npos) << path;
            }
        } break;
        case ReadlinkCase::Expect::failure_one_of: {
            if (!target) {
                auto matched{ false };
                for (const auto &e : c.errors) {
                    matched = matched || (target.error() == e);
                }

                EXPECT_TRUE(matched) << path << ": " << target.error().message();
            }
        } break;
        }
    }
}

auto get_readlink_testcases() noexcept -> const std::vector<ReadlinkCase> &
{
    const static std::vector<ReadlinkCase> readlink_cases{
        {
          "ExistingSymlink",
          { "b-file" },
          ReadlinkCase::Expect::target,
          "b/c/file",
        },
        {
          "AbsoluteSymlink",
          { "e" },
          ReadlinkCase::Expect::target,
          "/b/c/d/e",
        },
        {
          "DanglingSymlink",
          { "a-fake1" },
          ReadlinkCase::Expect::target,
          "a/fake",
        },
        {
          "LoopSymlink",
          { "loop/link" },
          ReadlinkCase::Expect::target,
          "a",
        },
        {
          "NonExistent",
          { "non-exist" },
          ReadlinkCase::Expect::failure_error,
          nullptr,
          nullptr,
          std::make_error_code(std::errc::no_such_file_or_directory),
        },
        {
          "NonSymlink",
          { "b/c", "b/c/file" },
          ReadlinkCase::Expect::failure,
        },
        {
          "ResolvingSymlinks",
          {
            "root-link1",
            "root-link2",
            "link1/target_abs",
            "link1/target_rel",
            "link2/link1_abs",
            "link2/link1_rel",
            "link3/target_abs",
            "link3/target_rel",
          },
          ReadlinkCase::Expect::success,
        },
        {
          "EscapeSymlink",
          { "escape-link1", "escape-link2" },
          ReadlinkCase::Expect::success_contains,
          nullptr,
          "target",
        },
        {
          "DeepDangling",
          {
            "dangling/a",
            "dangling/b/c",
            "dangling/c",
            "dangling/d/e",
            "dangling/e",
            "dangling/g",
          },
          ReadlinkCase::Expect::failure_one_of,
          nullptr,
          nullptr,
          { },
          {
            std::make_error_code(std::errc::no_such_file_or_directory),
            std::make_error_code(std::errc::permission_denied),
          },
        },
    };

    return readlink_cases;
}

INSTANTIATE_TEST_SUITE_P(ReadlinkCases,
                         VfsReadlink,
                         ::testing::ValuesIn(get_readlink_testcases()),
                         [](const ::testing::TestParamInfo<ReadlinkCase> &info) {
                             return std::string{ info.param.name };
                         });

struct RemoveCase
{
    const char *name;
    enum class Op : uint8_t {
        file,
        dir,
        all,
    } op;

    std::vector<const char *> paths;
    enum class Expect : uint8_t {
        success,
        failure,
        failure_error,
    } expect;
    std::error_code error{ };
};
LINYAPS_BOX_NAMED_PRINTER(RemoveCase)

class VfsRemove : public ::testing::TestWithParam<RemoveCase>
{
};

TEST_P(VfsRemove, Remove)
{
    const auto tree = BasicTree{ };
    auto root = tree.open();
    const auto &c = GetParam();

    for (const auto *path : c.paths) {
        SCOPED_TRACE(path);
        auto r = [&]() -> os::Result<void> {
            switch (c.op) {
            case RemoveCase::Op::file:
                return root.remove_file(path);
            case RemoveCase::Op::dir:
                return root.remove_dir(path);
            case RemoveCase::Op::all:
                return root.remove_all(path);
            }
            return { };
        }();

        switch (c.expect) {
        case RemoveCase::Expect::success: {
            ASSERT_TRUE(r) << path << ": " << r.error().message();
            EXPECT_FALSE(std::filesystem::exists(tree.base / path)) << path;
        } break;
        case RemoveCase::Expect::failure: {
            EXPECT_FALSE(r) << path << " should be rejected";
        } break;
        case RemoveCase::Expect::failure_error: {
            ASSERT_FALSE(r) << path;
            EXPECT_TRUE(r.error() == c.error) << path << ": " << r.error().message();
        } break;
        }
    }
}

auto get_remove_testcases() noexcept -> const std::vector<RemoveCase> &
{
    const static std::vector<RemoveCase> remove_cases{
        {
          "RemoveFiles",
          RemoveCase::Op::file,
          { "b-file", "b/c/file", "a-fake1" },
          RemoveCase::Expect::success,
        },
        {
          "RemoveFileParentdirTrailingSlash",
          RemoveCase::Op::file,
          { "b/c//file" },
          RemoveCase::Expect::success,
        },
        { "RemoveDir", RemoveCase::Op::dir, { "a" }, RemoveCase::Expect::success },
        { "RemoveDirTrailingSlash", RemoveCase::Op::dir, { "a/" }, RemoveCase::Expect::success },
        {
          "RemoveAlls",
          RemoveCase::Op::all,
          { "b", "deep-rmdir", "a-fake1", "non-exist" },
          RemoveCase::Expect::success,
        },
        {
          "RemoveFileOnDir",
          RemoveCase::Op::file,
          { "a" },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::is_a_directory),
        },
        {
          "RemoveFileNonExistent",
          RemoveCase::Op::file,
          { "non-exist" },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::no_such_file_or_directory),
        },
        {
          "RemoveFileNoexistTrailingSlash",
          RemoveCase::Op::file,
          { "abc/" },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::not_a_directory),
        },
        {
          "RemoveFileTrailingDotFamily",
          RemoveCase::Op::file,
          { "b/c/file/.", "b/c/file/.." },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::invalid_argument),
        },
        {
          "RemoveDirNonEmpty",
          RemoveCase::Op::dir,
          { "b" },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::directory_not_empty),
        },
        {
          "RemoveDirDanglingSymlink",
          RemoveCase::Op::dir,
          { "a-fake1" },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::not_a_directory),
        },
        {
          "RemoveDirTrailingDotFamily",
          RemoveCase::Op::dir,
          { "b/c/.", "b/c/.." },
          RemoveCase::Expect::failure_error,
          std::make_error_code(std::errc::invalid_argument),
        },
        {
          "RemoveFileTrailingSlash",
          RemoveCase::Op::file,
          { "b/c/file/" },
          RemoveCase::Expect::failure,
        },
        { "RemoveDirOnFile", RemoveCase::Op::dir, { "b/c/file" }, RemoveCase::Expect::failure },
        {
          "RemoveAllRootPath",
          RemoveCase::Op::all,
          { "/", ".", ".." },
          RemoveCase::Expect::failure,
        },
    };

    return remove_cases;
}

INSTANTIATE_TEST_SUITE_P(RemoveCases,
                         VfsRemove,
                         ::testing::ValuesIn(get_remove_testcases()),
                         [](const ::testing::TestParamInfo<RemoveCase> &info) {
                             return std::string{ info.param.name };
                         });

struct RenameCase
{
    const char *name;
    std::vector<std::pair<const char *, const char *>> moves;
    os::sys::rename_flag flag{ os::sys::rename_flag::none };
    enum class Expect : uint8_t {
        success,
        success_optional,
        failure,
        failure_error,
        exchange,
    } expect;
    const char *dst_exists{ nullptr };
    const char *src_gone{ nullptr };
    std::error_code error{ };
};
LINYAPS_BOX_NAMED_PRINTER(RenameCase)

class VfsRename : public ::testing::TestWithParam<RenameCase>
{
};

TEST_P(VfsRename, Rename)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    const auto &c = GetParam();

    for (const auto &[src, dst] : c.moves) {
        SCOPED_TRACE(std::string{ src } + " -> " + dst);
        auto r = root.rename(src, dst, c.flag);
        switch (c.expect) {
        case RenameCase::Expect::success: {
            ASSERT_TRUE(r) << src << " -> " << dst << ": " << r.error().message();
            if (c.dst_exists != nullptr) {
                EXPECT_TRUE(std::filesystem::exists(tree.base / c.dst_exists)) << c.dst_exists;
            }

            if (c.src_gone != nullptr) {
                EXPECT_FALSE(std::filesystem::exists(tree.base / c.src_gone)) << c.src_gone;
            }
        } break;
        case RenameCase::Expect::success_optional: {
            if (r) {
                if (c.dst_exists != nullptr) {
                    EXPECT_TRUE(std::filesystem::exists(tree.base / c.dst_exists)) << c.dst_exists;
                }

                if (c.src_gone != nullptr) {
                    EXPECT_FALSE(std::filesystem::exists(tree.base / c.src_gone)) << c.src_gone;
                }
            }
        } break;
        case RenameCase::Expect::failure: {
            EXPECT_FALSE(r) << src << " -> " << dst << " should be rejected";
        } break;
        case RenameCase::Expect::failure_error: {
            ASSERT_FALSE(r) << src << " -> " << dst;
            EXPECT_TRUE(r.error() == c.error)
              << src << " -> " << dst << ": " << r.error().message();
        } break;
        case RenameCase::Expect::exchange: {
            ASSERT_TRUE(r) << src << " <-> " << dst << ": " << r.error().message();
            EXPECT_TRUE(std::filesystem::is_symlink(tree.base / src)) << src;
            EXPECT_TRUE(std::filesystem::is_directory(tree.base / dst)) << dst;
        } break;
        }
    }
}

auto get_rename_testcases() noexcept -> const std::vector<RenameCase> &
{
    const static std::vector<RenameCase> rename_cases{
        {
          "Dir",
          { { "a", "aa" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success,
          "aa",
          "a",
        },
        {
          "NonEmptyDir",
          { { "b", "bb" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success,
          "bb",
          "b",
        },
        {
          "File",
          { { "b/c/file", "bb-file" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success,
          "bb-file",
          "b/c/file",
        },
        {
          "Exchange",
          { { "a", "e" } },
          os::sys::rename_flag::exchange,
          RenameCase::Expect::exchange,
        },
        {
          "ExchangeNonExistent",
          { { "a", "aa" } },
          os::sys::rename_flag::exchange,
          RenameCase::Expect::failure_error,
          nullptr,
          nullptr,
          std::make_error_code(std::errc::no_such_file_or_directory),
        },
        {
          "FileTrailingSlashSrc",
          { { "b/c/file/", "aa" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::failure_error,
          nullptr,
          nullptr,
          std::make_error_code(std::errc::not_a_directory),
        },
        {
          "FileTrailingSlashDst",
          { { "b/c/file", "aa/" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::failure,
        },
        {
          "DirTrailingSlashDst",
          { { "a", "aa/" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success_optional,
          "aa",
          "a",
        },
        {
          "ParentdirTrailingSlash",
          { { "b/c//d", "aa" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success,
          "aa",
        },
        {
          "DirTrailingSlashSrc",
          { { "a/", "aa" } },
          os::sys::rename_flag::none,
          RenameCase::Expect::success,
          "aa",
          "a",
        },
        {
          "Noreplace",
          { { "a", "aa" } },
          os::sys::rename_flag::noreplace,
          RenameCase::Expect::success,
          "aa",
          "a",
        },
        {
          "NoreplaceEexist",
          { { "a", "e" } },
          os::sys::rename_flag::noreplace,
          RenameCase::Expect::failure_error,
          nullptr,
          nullptr,
          std::make_error_code(std::errc::file_exists),
        },
        {
          "ExchangeTrailingSlashFailures",
          { { "a/", "e/" }, { "a", "e/" } },
          os::sys::rename_flag::exchange,
          RenameCase::Expect::failure,
        },
        {
          "TrailingDotDotdotFailures",
          { { "a/.", "aa" }, { "a", "aa/." }, { "b/c/..", "aa" }, { "a", "aa/.." } },
          os::sys::rename_flag::none,
          RenameCase::Expect::failure_error,
          nullptr,
          nullptr,
          std::make_error_code(std::errc::invalid_argument),
        },
    };

    return rename_cases;
}

INSTANTIATE_TEST_SUITE_P(RenameCases,
                         VfsRename,
                         ::testing::ValuesIn(get_rename_testcases()),
                         [](const ::testing::TestParamInfo<RenameCase> &info) {
                             return std::string{ info.param.name };
                         });

struct CreateFileCase
{
    const char *name;
    std::vector<const char *> paths;
    utils::bitflags<os::sys::open_flag> flags{ os::sys::open_flag::none };
    std::filesystem::perms perm = os::default_new_file_perm;
    enum class Expect : uint8_t {
        success_exists,
        success_regular,
        failure,
        failure_error,
    } expect;
    std::error_code error{ };
    std::function<void(const TempDir &)> prepare{ }; // NOLINT
};
LINYAPS_BOX_NAMED_PRINTER(CreateFileCase)

class VfsCreateFile : public ::testing::TestWithParam<CreateFileCase>
{
};

TEST_P(VfsCreateFile, CreateFile)
{
    const auto &c = GetParam();

    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    if (c.prepare) {
        c.prepare(d);
    }

    for (const auto *path : c.paths) {
        SCOPED_TRACE(path);
        auto fd = root.create_file(path, c.flags, c.perm);
        switch (c.expect) {
        case CreateFileCase::Expect::success_exists: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            EXPECT_TRUE(std::filesystem::exists(d.path() / path));

        } break;
        case CreateFileCase::Expect::success_regular: {
            if (!fd) {
                const auto err = fd.error();
                if (err == std::errc::operation_not_supported) {
                    GTEST_SKIP() << "Filesystem does not support O_TMPFILE: " << err.message();
                }

                FAIL() << "Failed to create tmpfile due to unexpected error: " << err.message();
            }

            auto st = os::fstat(fd->ref());
            ASSERT_TRUE(st);
            EXPECT_TRUE(S_ISREG(st->st_mode));
        } break;
        case CreateFileCase::Expect::failure: {
            EXPECT_FALSE(fd) << path << " should be rejected";
        } break;
        case CreateFileCase::Expect::failure_error: {
            ASSERT_FALSE(fd) << path;
            EXPECT_TRUE(fd.error() == c.error) << path << ": " << fd.error().message();
        } break;
        }
    }
}

auto get_create_file_testcases() noexcept // NOLINT(readability-function-cognitive-complexity)
  -> const std::vector<CreateFileCase> &
{
    const static std::vector<CreateFileCase> create_file_cases{
        {
          "CreateFileSuccess",
          { "newfile", "b/c//foobar" },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::success_exists,
          { },
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "b" / "c");
          },
        },
        {
          "CreateFileTrailingSlash",
          { "foobar/" },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::failure,
        },
        {
          "CreateFileTrailingDot",
          { "foobar/." },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::failure_error,
          std::make_error_code(std::errc::invalid_argument),
        },
        {
          "CreateFileOnDir",
          { "mydir" },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::failure_error,
          std::make_error_code(std::errc::is_a_directory),
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "mydir");
          },
        },
        {
          "CreateFileOnSymlink",
          { "mylink" },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::failure,
          { },
          [](const TempDir &d) {
              std::error_code ec;
              std::filesystem::create_symlink("target", d.path() / "mylink", ec);
              ASSERT_FALSE(ec);
          },
        },
        {
          "CreateFileOexclOnSymlink",
          { "mylink" },
          os::sys::open_flag::exclusive,
          os::default_new_file_perm,
          CreateFileCase::Expect::failure_error,
          std::make_error_code(std::errc::file_exists),
          [](const TempDir &d) {
              std::error_code ec;
              std::filesystem::create_symlink("target", d.path() / "mylink", ec);
              ASSERT_FALSE(ec);
          },
        },
        {
          "CreateFileRootPath",
          { "/", ".", ".." },
          { },
          os::default_new_file_perm,
          CreateFileCase::Expect::failure,
        },
        {
          "CreateFileTmpfileNoent",
          { "dir/newfile" },
          os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec | os::sys::open_flag::exclusive,
          tmpfile_perm,
          CreateFileCase::Expect::failure_error,
          std::make_error_code(std::errc::no_such_file_or_directory),
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "dir");
          },
        },
        {
          "CreateFileTmpfileOnFile",
          { "afile" },
          os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec | os::sys::open_flag::exclusive,
          tmpfile_perm,
          CreateFileCase::Expect::failure_error,
          std::make_error_code(std::errc::not_a_directory),
          [](const TempDir &d) {
              write_file(d.path() / "afile", "x");
          },
        },
        {
          "CreateFileTmpfile",
          { "tmpdir" },
          os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec | os::sys::open_flag::exclusive,
          tmpfile_perm,
          CreateFileCase::Expect::success_regular,
          { },
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "tmpdir");
          },
        },
    };

    return create_file_cases;
}

INSTANTIATE_TEST_SUITE_P(CreateFileCases,
                         VfsCreateFile,
                         ::testing::ValuesIn(get_create_file_testcases()),
                         [](const ::testing::TestParamInfo<CreateFileCase> &info) {
                             return std::string{ info.param.name };
                         });

struct CreateSymlinkCase
{
    const char *name;
    std::vector<const char *> paths;
    const char *target{ "/target" };
    enum class Expect : uint8_t {
        success_symlink,
        failure,
        failure_error,
    } expect;
    const char *link_target{ nullptr };
    std::error_code error{ };
    std::function<void(const TempDir &)> prepare{ }; // NOLINT
};
LINYAPS_BOX_NAMED_PRINTER(CreateSymlinkCase)

class VfsCreateSymlink : public ::testing::TestWithParam<CreateSymlinkCase>
{
};

TEST_P(VfsCreateSymlink, Symlink)
{
    const auto &c = GetParam();

    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    if (c.prepare) {
        c.prepare(d);
    }

    for (const auto *path : c.paths) {
        SCOPED_TRACE(path);
        auto r = root.create(path, infra::symlink_spec{ c.target });
        switch (c.expect) {
        case CreateSymlinkCase::Expect::success_symlink: {
            ASSERT_TRUE(r) << path << ": " << r.error().message();
            EXPECT_TRUE(std::filesystem::is_symlink(d.path() / path)) << path;
            if (c.link_target != nullptr) {
                std::error_code ec;
                const auto target = std::filesystem::read_symlink(d.path() / path, ec);
                ASSERT_FALSE(ec);
                EXPECT_EQ(target, c.link_target);
            }
        } break;
        case CreateSymlinkCase::Expect::failure: {
            EXPECT_FALSE(r) << path << " should be rejected";
        } break;
        case CreateSymlinkCase::Expect::failure_error: {
            ASSERT_FALSE(r) << path;
            EXPECT_TRUE(r.error() == c.error) << path << ": " << r.error().message();
        } break;
        }
    }
}

auto get_create_symlink_testcases() noexcept -> const std::vector<CreateSymlinkCase> &
{
    const static std::vector<CreateSymlinkCase> create_symlink_cases{
        {
          "SymlinkSuccess",
          { "mylink", "b/c//foobar" },
          "/target",
          CreateSymlinkCase::Expect::success_symlink,
          "/target",
          { },
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "b" / "c");
          },
        },
        {
          "SymlinkInvalidShape",
          { "mylink/", "foobar/.", "foobar/..", "/" },
          "/target",
          CreateSymlinkCase::Expect::failure_error,
          nullptr,
          std::make_error_code(std::errc::invalid_argument),
        },
        { "SymlinkRootPathVariants", { ".", ".." }, "/target", CreateSymlinkCase::Expect::failure },
    };

    return create_symlink_cases;
}

INSTANTIATE_TEST_SUITE_P(CreateSymlinkCases,
                         VfsCreateSymlink,
                         ::testing::ValuesIn(get_create_symlink_testcases()),
                         [](const ::testing::TestParamInfo<CreateSymlinkCase> &info) {
                             return std::string{ info.param.name };
                         });

struct CreateHardlinkCase
{
    const char *name;
    std::vector<std::pair<const char *, const char *>> links;
    enum class Expect : uint8_t {
        success_exists,
        success_optional_symlink,
        failure,
        failure_error,
        failure_one_of,
    } expect;
    std::error_code error{ };
    std::vector<std::error_code> errors{ };          // NOLINT
    std::function<void(const TempDir &)> prepare{ }; // NOLINT
};
LINYAPS_BOX_NAMED_PRINTER(CreateHardlinkCase)

class VfsCreateHardlink : public ::testing::TestWithParam<CreateHardlinkCase>
{
};

TEST_P(VfsCreateHardlink, Hardlink)
{
    const auto &c = GetParam();

    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    if (c.prepare) {
        c.prepare(d);
    }

    for (const auto &[dst, target] : c.links) {
        SCOPED_TRACE(std::string{ dst } + " -> " + target);
        auto r = root.create(dst, infra::hardlink_spec{ target });
        switch (c.expect) {
        case CreateHardlinkCase::Expect::success_exists: {
            ASSERT_TRUE(r) << dst << " -> " << target << ": " << r.error().message();
            EXPECT_TRUE(std::filesystem::exists(d.path() / dst));
        } break;
        case CreateHardlinkCase::Expect::success_optional_symlink: {
            if (r) {
                EXPECT_TRUE(std::filesystem::is_symlink(d.path() / dst));
            }
        } break;
        case CreateHardlinkCase::Expect::failure: {
            EXPECT_FALSE(r) << dst << " -> " << target << " should be rejected";
        } break;
        case CreateHardlinkCase::Expect::failure_error: {
            ASSERT_FALSE(r) << dst << " -> " << target;
            EXPECT_TRUE(r.error() == c.error) << dst << ": " << r.error().message();
        } break;
        case CreateHardlinkCase::Expect::failure_one_of: {
            ASSERT_FALSE(r) << dst << " -> " << target;
            bool matched{ false };
            for (const auto &e : c.errors) {
                matched = matched || (r.error() == e);
            }

            EXPECT_TRUE(matched) << dst << ": " << r.error().message();
        } break;
        }
    }
}

auto get_create_hardlink_testcases() noexcept -> const std::vector<CreateHardlinkCase> &
{
    const static std::vector<CreateHardlinkCase> create_hardlink_cases{
        {
          "HardlinkSuccessFamily",
          { { "dst", "src" }, { "b/c//dst", "src" } },
          CreateHardlinkCase::Expect::success_exists,
          { },
          { },
          [](const TempDir &d) {
              write_file(d.path() / "src", "data");
              std::filesystem::create_directories(d.path() / "b" / "c");
          },
        },
        {
          "HardlinkInvalidShape",
          { { "dst", "src/" }, { "dst/", "src" } },
          CreateHardlinkCase::Expect::failure_error,
          std::make_error_code(std::errc::invalid_argument),
          { },
          [](const TempDir &d) {
              write_file(d.path() / "src", "data");
          },
        },
        {
          "HardlinkToSymlink",
          { { "hardlink", "link" } },
          CreateHardlinkCase::Expect::success_optional_symlink,
          { },
          { },
          [](const TempDir &d) {
              std::error_code ec;
              std::filesystem::create_symlink("target", d.path() / "link", ec);
              ASSERT_FALSE(ec);
          },
        },
        {
          "HardlinkToDanglingSymlink",
          { { "hardlink", "dangling" } },
          CreateHardlinkCase::Expect::success_optional_symlink,
          { },
          { },
          [](const TempDir &d) {
              std::error_code ec;
              std::filesystem::create_symlink("nonexistent", d.path() / "dangling", ec);
              ASSERT_FALSE(ec);
          },
        },
        {
          "HardlinkToDir",
          { { "hardlink", "mydir" } },
          CreateHardlinkCase::Expect::failure_one_of,
          { },
          {
            std::make_error_code(std::errc::operation_not_permitted),
            std::make_error_code(std::errc::permission_denied),
          },
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "mydir");
          },
        },
    };

    return create_hardlink_cases;
}

INSTANTIATE_TEST_SUITE_P(CreateHardlinkCases,
                         VfsCreateHardlink,
                         ::testing::ValuesIn(get_create_hardlink_testcases()),
                         [](const ::testing::TestParamInfo<CreateHardlinkCase> &info) {
                             return std::string{ info.param.name };
                         });

struct CreateDirectoriesCase
{
    const char *name;
    std::vector<std::pair<const char *, const char *>> mkdirs;
    std::filesystem::perms perm = os::default_dir_perm;
    enum class Expect : uint8_t {
        success,
        failure,
        failure_error,
    } expect;
    std::error_code error{ };
};
LINYAPS_BOX_NAMED_PRINTER(CreateDirectoriesCase)

class VfsCreateDirectories : public ::testing::TestWithParam<CreateDirectoriesCase>
{
};

TEST_P(VfsCreateDirectories, MkdirAll)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    const auto &c = GetParam();

    for (const auto &[path, check_dir] : c.mkdirs) {
        SCOPED_TRACE(path);
        auto fd = root.create_directories(path, c.perm);
        switch (c.expect) {
        case CreateDirectoriesCase::Expect::success: {
            ASSERT_TRUE(fd) << path << ": " << fd.error().message();
            if (check_dir != nullptr) {
                EXPECT_TRUE(std::filesystem::is_directory(tree.base / check_dir)) << check_dir;
            }
        } break;
        case CreateDirectoriesCase::Expect::failure: {
            EXPECT_FALSE(fd) << path << " should be rejected";
        } break;
        case CreateDirectoriesCase::Expect::failure_error: {
            EXPECT_FALSE(fd);
            EXPECT_TRUE(fd.error() == c.error) << path << ": " << fd.error().message();
        } break;
        }
    }
}

auto get_create_directories_testcases() noexcept -> const std::vector<CreateDirectoriesCase> &
{
    const static std::vector<CreateDirectoriesCase> create_directories_cases{
        {
          "MkdirAllSuccessFamily",
          {
            { "a", nullptr },
            { "a/b/c/d/e/f/g/h/i/j", "a/b/c/d/e/f/g/h/i/j" },
            { "x/y/z/", "x/y/z" },
            { "e/foo", "b/c/d/e/foo" },
            { "b/c/../c/./d/e/f/g/h", "b/c/d/e/f/g/h" },
            { "e/../dd/ee/ff", "b/c/d/dd/ee/ff" },
            { "link1/target_abs/foo", "target/foo" },
            { "link1/target_rel/foo", "target/foo" },
          },
          mkdirs_perm,
          CreateDirectoriesCase::Expect::success,
        },
        {
          "MkdirAllTrailingDot",
          { { "foobar/.", "foobar" } },
          os::default_dir_perm,
          CreateDirectoriesCase::Expect::success,
        },
        {
          "MkdirAllNonDirFamily",
          {
            { "b-file", nullptr },
            { "b/c/file/subdir", nullptr },
            { "a-fake1/foo", nullptr },
            { "loop/link", nullptr },
          },
          mkdirs_perm,
          CreateDirectoriesCase::Expect::failure_error,
          std::make_error_code(std::errc::not_a_directory),
        },
        {
          "MkdirAllTrailingDotdot",
          { { "foobar/..", nullptr } },
          os::default_dir_perm,
          CreateDirectoriesCase::Expect::failure,
        },
        {
          "MkdirAllDotdotInNonexisting",
          { { "a/b/c/d/e/f/g/h/i/j/k/../lmnop", nullptr } },
          mkdirs_perm,
          CreateDirectoriesCase::Expect::failure_error,
          std::make_error_code(std::errc::no_such_file_or_directory),
        },
    };

    return create_directories_cases;
}

INSTANTIATE_TEST_SUITE_P(CreateDirectoriesCases,
                         VfsCreateDirectories,
                         ::testing::ValuesIn(get_create_directories_testcases()),
                         [](const ::testing::TestParamInfo<CreateDirectoriesCase> &info) {
                             return std::string{ info.param.name };
                         });

struct CreateDirectoryCase
{
    const char *name;
    const char *path;
    std::filesystem::perms perm{ os::default_dir_perm };
    enum class Expect : uint8_t {
        success_dir,
        failure_error,
    } expect;

    const char *check_dir{ nullptr };
    std::error_code error{ };
    std::function<void(const TempDir &)> prepare{ }; // NOLINT
};
LINYAPS_BOX_NAMED_PRINTER(CreateDirectoryCase)

class VfsCreateDirectory : public ::testing::TestWithParam<CreateDirectoryCase>
{
};

TEST_P(VfsCreateDirectory, CreateDirectory)
{
    const auto &c = GetParam();
    SCOPED_TRACE(c.path);

    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    if (c.prepare) {
        c.prepare(d);
    }

    auto fd = root.create_directory(c.path, c.perm);
    switch (c.expect) {
    case CreateDirectoryCase::Expect::success_dir: {
        ASSERT_TRUE(fd) << c.path << ": " << fd.error().message();
        EXPECT_TRUE(std::filesystem::is_directory(d.path() / c.check_dir)) << c.check_dir;
    } break;
    case CreateDirectoryCase::Expect::failure_error: {
        EXPECT_FALSE(fd);
        EXPECT_TRUE(fd.error() == c.error) << c.path << ": " << fd.error().message();
    } break;
    }
}

auto get_create_directory_testcases() noexcept -> const std::vector<CreateDirectoryCase> &
{
    const static std::vector<CreateDirectoryCase> create_directory_cases{
        {
          "CreateDirectory",
          "newdir",
          os::default_dir_perm,
          CreateDirectoryCase::Expect::success_dir,
          "newdir",
        },
        {
          "CreateDirectoryExisting",
          "a",
          os::default_dir_perm,
          CreateDirectoryCase::Expect::failure_error,
          nullptr,
          std::make_error_code(std::errc::file_exists),
          [](const TempDir &d) {
              std::filesystem::create_directories(d.path() / "a");
          },
        },
        {
          "CreateDirectoryNonExistentParent",
          "a/b/c",
          os::default_dir_perm,
          CreateDirectoryCase::Expect::failure_error,
          nullptr,
          std::make_error_code(std::errc::no_such_file_or_directory),
        },
    };

    return create_directory_cases;
}

INSTANTIATE_TEST_SUITE_P(CreateDirectoryCases,
                         VfsCreateDirectory,
                         ::testing::ValuesIn(get_create_directory_testcases()),
                         [](const ::testing::TestParamInfo<CreateDirectoryCase> &info) {
                             return std::string{ info.param.name };
                         });

} // namespace

TEST(VfsRoot, OpenFollowsSymlinkedRootfs)
{
    const TempDir d;
    const auto link = d.path() / "rootlink";
    std::error_code ec;
    std::filesystem::create_symlink("/", link, ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(link);
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->valid());
    EXPECT_EQ(root->ref().current_path(), "/");
}

TEST(VfsRoot, OpenVariants)
{
    // "/" directly.
    {
        auto root = infra::Root::open("/");
        ASSERT_TRUE(root.has_value());
        EXPECT_TRUE(root->valid());
    }

    // Through a symlinked parent directory.
    {
        const TempDir d;
        const auto real = d.path() / "real";
        std::filesystem::create_directories(real / "rootfs");
        const auto link = d.path() / "link";
        std::error_code ec;
        std::filesystem::create_symlink("real", link, ec);
        ASSERT_FALSE(ec) << ec.message();

        auto root = infra::Root::open(link / "rootfs");
        ASSERT_TRUE(root.has_value());
        EXPECT_TRUE(root->valid());
    }

    // With a non-lexical ".." component.
    {
        const TempDir d;
        std::filesystem::create_directories(d.path() / "x" / "rootfs");
        auto root = infra::Root::open(d.path() / "x" / ".." / "x" / "rootfs");
        ASSERT_TRUE(root.has_value());
        EXPECT_TRUE(root->valid());
    }
}

TEST(VfsRoot, ReopenSucceeds)
{
    const TempDir d;
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root);
    auto r = root->reopen();
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(root->valid());
    EXPECT_EQ(root->ref().current_path().string(), d.path().string());
}

TEST(VfsUnscoped, OpenReadsHost)
{
    const TempDir d;
    write_file(d.path() / "u.txt", "u");
    auto fd =
      os::open(d.path() / "u.txt",
               os::sys::open_option(os::sys::open_flag{ }, os::sys::access_mode::read_only));
    ASSERT_TRUE(fd.has_value());
    std::array<char, 2> buf{ };
    EXPECT_EQ(::read(fd->get(), buf.data(), 1), 1);
    EXPECT_EQ(buf[0], 'u');
}

TEST(VfsScoped, DotdotClampedAtRoot)
{
    const TempDir d;
    write_file(d.path() / "inside.txt", "ok");
    const auto sibling = std::filesystem::path("/tmp/linyaps-box-vfs-sibling-marker");
    write_file(sibling, "secret");
    const auto root = infra::Root::open(d.path()).value();
    auto fd = root.open("../linyaps-box-vfs-sibling-marker",
                        { os::sys::open_flag{ }, os::sys::access_mode::path });
    if (fd.has_value()) {
        std::array<char, 16> buf{ };
        std::ignore = ::read(fd->get(), buf.data(), 6);
        EXPECT_STRNE(buf.data(), "secret") << "escaped the root via ..";
    }

    const auto inside =
      root.open("inside.txt", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(inside.has_value());
    std::ignore = os::unlinkat(utils::file_descriptor_ref::cwd(), sibling, os::sys::at_flag::none);
}

TEST(VfsScoped, SymlinkEscapeRejected)
{
    const TempDir d;
    const auto outside = std::filesystem::path("/tmp/linyaps-box-vfs-outside-marker");
    write_file(outside, "outside-secret");
    std::error_code ec;
    std::filesystem::create_symlink(outside, d.path() / "escape", ec);
    ASSERT_FALSE(ec);
    const auto root = infra::Root::open(d.path()).value();
    auto fd = root.open("escape", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd.has_value()) << "should reject symlink escape";
    if (fd) {
        std::array<char, 16> buf{ };
        std::ignore = ::read(fd->get(), buf.data(), 7);
        EXPECT_STRNE(buf.data(), "outside-secret");
    }
    std::ignore = os::unlinkat(utils::file_descriptor_ref::cwd(), outside, os::sys::at_flag::none);
}

TEST(VfsScoped, ResolvedPathsNeverEscapeRoot)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    const auto base = tree.base.native();

    const std::array<const char *, 10> paths{
        "../escape",
        "../../etc/passwd",
        "/etc/passwd",
        "//etc/passwd",
        "a/../../escape",
        "b/c/../../../../../../etc/passwd",
        "root-link1/etc/passwd",
        "root-link2/etc/passwd",
        "e/../../../../etc/passwd",
        "loop/link/../../../etc/passwd",
    };

    for (const auto *path : paths) {
        SCOPED_TRACE(path);
        const auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        if (!fd) {
            continue; // rejected — containment preserved
        }

        const auto resolved = std::filesystem::read_symlink(std::filesystem::path{ "/proc/self/fd" }
                                                            / std::to_string(fd->get()))
                                .native();
        EXPECT_EQ(resolved.compare(0, base.size(), base), 0)
          << path << " escaped the root to " << resolved;
    }
}

TEST(VfsScoped, RelativeSymlinkResolves)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "a");
    write_file(d.path() / "a" / "real.txt", "inside");
    std::error_code ec;
    std::filesystem::create_symlink("real.txt", d.path() / "a" / "link", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto fd = root->open("a/link", { os::sys::open_flag{ }, os::sys::access_mode::read_only });
    ASSERT_TRUE(fd.has_value());
    std::array<char, 16> buf{ };
    EXPECT_EQ(::read(fd->get(), buf.data(), 6), 6);
    EXPECT_EQ(std::string_view(buf.data(), 6), "inside");
}

TEST(VfsCreate, Fifo)
{
    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    auto r = root.create("myfifo", infra::fifo_spec{ os::default_new_file_perm });
    ASSERT_TRUE(r) << r.error().message();
    auto st = os::fstatat(linyaps_box::utils::file_descriptor_ref::cwd(),
                          d.path() / "myfifo",
                          os::sys::at_flag::symlink_nofollow);
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISFIFO(st->st_mode));
}

TEST(VfsCreate, MkdirAllRootPath)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    for (const auto *path : { "/", "." }) {
        auto fd = root.create_directories(path, os::default_dir_perm);
        ASSERT_TRUE(fd) << path << " should succeed: " << fd.error().message();
    }

    // ".." resolves to root but may fail EACCES from containment check
    auto fd = root.create_directories("..", os::default_dir_perm);
    if (!fd) {
        EXPECT_TRUE(fd.error() == std::make_error_code(std::errc::permission_denied));
    }
}

TEST(VfsCreate, MkdirAllInvalidMode)
{
    const TempDir d;
    const auto root = infra::Root::open(d.path()).value();
    // setuid bit should be rejected
    auto fd = root.create_directories("foo", invalidmode_perm);
    EXPECT_FALSE(fd) << "setuid mode should be EINVAL";
    EXPECT_TRUE(fd.error() == std::make_error_code(std::errc::invalid_argument));
}

TEST(VfsRename, RenameRootPath)
{
    const auto tree = BasicTree{ };
    const auto root = tree.open();
    for (const auto *src : { "/", ".", ".." }) {
        auto r = root.rename(src, "aa");
        EXPECT_FALSE(r) << src << " -> aa should be rejected";
    }
    for (const auto *dst : { "/", ".", ".." }) {
        auto r = root.rename("a", dst);
        EXPECT_FALSE(r) << "a -> " << dst << " should be rejected";
    }
}
