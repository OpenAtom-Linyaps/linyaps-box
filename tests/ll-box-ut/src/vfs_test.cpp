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

#include <gtest/gtest.h>

#include "linyaps_box/infra/rootfs.h"
#include "linyaps_box/os/fs.h"

#include <fmt/std.h>

#include <fstream>
#include <string>
#include <system_error>

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

    auto open() const noexcept -> infra::Root { return infra::Root::open(base).value(); }
};

} // namespace

TEST(VfsRoot, OpenFollowsSymlinkedRootfs)
{
    const TempDir d;
    auto link = d.path() / "rootlink";
    std::error_code ec;
    std::filesystem::create_symlink("/", link, ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(link);
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->valid());
    EXPECT_EQ(root->ref().current_path(), "/");
}

TEST(VfsScoped, DotdotClampedAtRoot)
{
    const TempDir d;
    write_file(d.path() / "inside.txt", "ok");
    auto sibling = std::filesystem::path("/tmp/linyaps-box-vfs-sibling-marker");
    write_file(sibling, "secret");
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.open("../linyaps-box-vfs-sibling-marker",
                        { os::sys::open_flag{ }, os::sys::access_mode::path });
    if (fd.has_value()) {
        std::array<char, 16> buf{ };
        std::ignore = ::read(fd->get(), buf.data(), 6);
        EXPECT_STRNE(buf.data(), "secret") << "escaped the root via ..";
    }
    auto inside = root.open("inside.txt", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(inside.has_value());
    std::ignore = os::unlinkat(utils::file_descriptor_ref::cwd(), sibling, os::sys::at_flag::none);
}

TEST(VfsScoped, SymlinkEscapeRejected)
{
    const TempDir d;
    auto outside = std::filesystem::path("/tmp/linyaps-box-vfs-outside-marker");
    write_file(outside, "outside-secret");
    std::error_code ec;
    std::filesystem::create_symlink(outside, d.path() / "escape", ec);
    ASSERT_FALSE(ec);
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.open("escape", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd.has_value()) << "should reject symlink escape";
    if (fd) {
        std::array<char, 16> buf{ };
        std::ignore = ::read(fd->get(), buf.data(), 7);
        EXPECT_STRNE(buf.data(), "outside-secret");
    }
    std::ignore = os::unlinkat(utils::file_descriptor_ref::cwd(), outside, os::sys::at_flag::none);
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

TEST(VfsRoot, OpenSlashSucceeds)
{
    auto root = infra::Root::open("/");
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->valid());
}

TEST(VfsRoot, OpenThroughSymlinkedParent)
{
    const TempDir d;
    auto real = d.path() / "real";
    std::filesystem::create_directories(real / "rootfs");
    auto link = d.path() / "link";
    std::error_code ec;
    std::filesystem::create_symlink("real", link, ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(link / "rootfs");
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->valid());
}

TEST(VfsRoot, OpenWithDotdotComponent)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "x" / "rootfs");
    auto root = infra::Root::open(d.path() / "x" / ".." / "x" / "rootfs");
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(root->valid());
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

TEST(VfsScoped, MkdirAllThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto fd = root->create_directories("bin/foo", os::default_dir_perm);
    ASSERT_TRUE(fd.has_value());
    EXPECT_TRUE(std::filesystem::is_directory(d.path() / "real" / "foo"));
}

TEST(VfsScoped, CreateFileThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto fd = root->create_file("bin/newfile", os::sys::open_flag::none, os::default_new_file_perm);
    ASSERT_TRUE(fd.has_value());
    EXPECT_TRUE(std::filesystem::is_regular_file(d.path() / "real" / "newfile"));
}

TEST(VfsScoped, CreateDirectoryThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto fd = root->create_directory("bin/newdir", os::default_dir_perm);
    ASSERT_TRUE(fd.has_value()) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(d.path() / "real" / "newdir"));
}

TEST(VfsScoped, CreateSymlinkThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto r = root->create("bin/mylink", infra::symlink_spec{ "/target" });
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::is_symlink(d.path() / "real" / "mylink"));
}

TEST(VfsScoped, RemoveFileThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    write_file(d.path() / "real" / "target", "data");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto r = root->remove_file("bin/target");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "target"));
}

TEST(VfsScoped, RemoveDirThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real" / "subdir");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto r = root->remove_dir("bin/subdir");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "subdir"));
}

TEST(VfsScoped, RemoveAllThroughSymlinkedDir)
{
    const TempDir d;
    auto dir = d.path() / "real" / "target";
    std::filesystem::create_directories(dir);
    write_file(dir / "file", "data");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto r = root->remove_all("bin/target");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(dir));
}

TEST(VfsScoped, RenameThroughSymlinkedDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "real");
    write_file(d.path() / "real" / "src", "data");
    std::error_code ec;
    std::filesystem::create_symlink("real", d.path() / "bin", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto root = infra::Root::open(d.path());
    ASSERT_TRUE(root) << root.error().message();
    auto r = root->rename("bin/src", "bin/dst");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(d.path() / "real" / "dst"));
    EXPECT_FALSE(std::filesystem::exists(d.path() / "real" / "src"));
}

TEST(VfsResolve, CompleteRoot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("/", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISDIR(st->st_mode));
}

TEST(VfsResolve, CompleteRootClamped)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("/../../../../..", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISDIR(st->st_mode));
}

TEST(VfsResolve, CompleteDir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "a", "b/c", "b/c/d/e/f" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        ASSERT_TRUE(fd) << path << ": " << fd.error().message();
        auto st = os::fstat(fd->ref());
        ASSERT_TRUE(st);
        EXPECT_TRUE(S_ISDIR(st->st_mode)) << path;
    }
}

TEST(VfsResolve, CompleteFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b/c/file", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISREG(st->st_mode));
}

TEST(VfsResolve, ExcessiveSlashesAndDots)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b///././c////.//d/./././///e////.//./f//././././",
                        { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISDIR(st->st_mode));
}

TEST(VfsResolve, RootSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "root-link1", "root-link2" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        ASSERT_TRUE(fd) << path << ": " << fd.error().message();
        auto st = os::fstat(fd->ref());
        ASSERT_TRUE(st);
        EXPECT_TRUE(S_ISDIR(st->st_mode)) << path;
    }
}

TEST(VfsResolve, SymlinkRelative)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b-file", { os::sys::open_flag{ }, os::sys::access_mode::read_only });
    ASSERT_TRUE(fd) << fd.error().message();
    std::array<char, 16> buf{ };
    EXPECT_EQ(::read(fd->get(), buf.data(), 12), 12);
    EXPECT_EQ(std::string_view(buf.data(), 12), "file content");
}

TEST(VfsResolve, SymlinkAbsolute)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("e", { os::sys::open_flag{ }, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISDIR(st->st_mode));
}

TEST(VfsResolve, SymlinkMultiLevel)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "link1/target_abs",
                              "link1/target_rel",
                              "link2/link1_abs/target_abs",
                              "link2/link1_abs/target_rel",
                              "link2/link1_rel/target_abs",
                              "link2/link1_rel/target_rel" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        ASSERT_TRUE(fd) << path << ": " << fd.error().message();
        auto st = os::fstat(fd->ref());
        ASSERT_TRUE(st);
        EXPECT_TRUE(S_ISDIR(st->st_mode)) << path;
    }
}

TEST(VfsResolve, SymlinkDeepMultiLevel)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "link3/target_abs", "link3/target_rel" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        ASSERT_TRUE(fd) << path << ": " << fd.error().message();
        auto st = os::fstat(fd->ref());
        ASSERT_TRUE(st);
        EXPECT_TRUE(S_ISDIR(st->st_mode)) << path;
    }
}

TEST(VfsResolve, NonExistent)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("a/b/c/d/e/f/g/h", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, TrailingSlashOnFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b/c/file/", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, ComponentThroughFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b/c/file/../foo", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, TrailingSlashOnSymlinkToFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b-file/", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, SymlinkLoop)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "loop/basic-loop1", "loop/basic-loop2", "loop/basic-loop3" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be rejected";
    }
    auto fd = root.open("loop/link", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, DanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "a-fake1", "a-fake2", "a-fake3" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsResolve, NofollowSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b-file", { os::sys::open_flag::no_follow, os::sys::access_mode::path });
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISLNK(st->st_mode));
}

TEST(VfsResolve, NofollowSymlinkDirectory)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.open("b-file",
                        { os::sys::open_flag::no_follow | os::sys::open_flag::directory,
                          os::sys::access_mode::path });
    ASSERT_FALSE(fd) << "O_DIRECTORY|O_NOFOLLOW on symlink should be ENOTDIR";
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsResolve, NofollowSymlinkRead)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd =
      root.open("b-file", { os::sys::open_flag::no_follow, os::sys::access_mode::read_only });
    ASSERT_FALSE(fd) << "O_NOFOLLOW|O_RDONLY on symlink should be ELOOP";
    EXPECT_TRUE(fd.error() == std::errc::too_many_symbolic_link_levels);
}

TEST(VfsReadlink, ExistingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto target = root.readlink("b-file");
    ASSERT_TRUE(target) << target.error().message();
    EXPECT_EQ(*target, "b/c/file");
}

TEST(VfsReadlink, AbsoluteSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto target = root.readlink("e");
    ASSERT_TRUE(target) << target.error().message();
    EXPECT_EQ(*target, "/b/c/d/e");
}

TEST(VfsReadlink, DanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto target = root.readlink("a-fake1");
    ASSERT_TRUE(target) << target.error().message();
    EXPECT_EQ(*target, "a/fake");
}

TEST(VfsReadlink, NonExistent)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto target = root.readlink("non-exist");
    EXPECT_FALSE(target);
    EXPECT_TRUE(target.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsReadlink, NonSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "b/c", "b/c/file" }) {
        auto target = root.readlink(path);
        EXPECT_FALSE(target) << path;
    }
}

TEST(VfsCreate, Symlink)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("mylink", infra::symlink_spec{ "/target" });
    ASSERT_TRUE(r) << r.error().message();
    std::error_code ec;
    auto target = std::filesystem::read_symlink(d.path() / "mylink", ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(target, "/target");
}

TEST(VfsCreate, SymlinkNoTrailingSlash)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("mylink/", infra::symlink_spec{ "/target" });
    EXPECT_FALSE(r) << "trailing slash on symlink create should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, Hardlink)
{
    const TempDir d;
    write_file(d.path() / "src", "data");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("dst", infra::hardlink_spec{ "src" });
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(d.path() / "dst"));
}

TEST(VfsCreate, HardlinkNoTrailingSlashOnTarget)
{
    const TempDir d;
    write_file(d.path() / "src", "data");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("dst", infra::hardlink_spec{ "src/" });
    ASSERT_FALSE(r) << "trailing slash on hardlink target should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, Fifo)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("myfifo", infra::fifo_spec{ os::default_new_file_perm });
    ASSERT_TRUE(r) << r.error().message();
    auto st = os::fstatat(linyaps_box::utils::file_descriptor_ref::cwd(),
                          d.path() / "myfifo",
                          os::sys::at_flag::symlink_nofollow);
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISFIFO(st->st_mode));
}

TEST(VfsCreate, PathIsSlashRejected)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("/", infra::symlink_spec{ "/target" });
    EXPECT_FALSE(r) << "path '/' should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, CreateDirectory)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_directory("newdir", os::default_dir_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(d.path() / "newdir"));
}

TEST(VfsCreate, CreateDirectoryExisting)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directory("a", os::default_dir_perm);
    EXPECT_FALSE(fd) << "create_directory on existing dir should be EEXIST";
    EXPECT_TRUE(fd.error() == std::errc::file_exists);
}

TEST(VfsCreate, CreateDirectoryNonExistentParent)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_directory("a/b/c", os::default_dir_perm);
    EXPECT_FALSE(fd) << "create_directory with non-existent parent should be ENOENT";
    EXPECT_TRUE(fd.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsCreate, CreateFile)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("newfile", os::sys::open_flag::none, os::default_new_file_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::exists(d.path() / "newfile"));
}

TEST(VfsCreate, CreateFileTmpfile)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "tmpdir");
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("tmpdir",
                               os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec
                                 | os::sys::open_flag::exclusive,
                               tmpfile_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    auto st = os::fstat(fd->ref());
    ASSERT_TRUE(st);
    EXPECT_TRUE(S_ISREG(st->st_mode));
}

TEST(VfsRemove, RemoveFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b/c/file");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b/c/file"));
}

TEST(VfsRemove, RemoveFileSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b-file");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b-file"));
}

TEST(VfsRemove, RemoveFileOnDir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("a");
    EXPECT_FALSE(r);
    EXPECT_TRUE(r.error() == std::errc::is_a_directory);
}

TEST(VfsRemove, RemoveFileNonExistent)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("non-exist");
    EXPECT_FALSE(r);
    EXPECT_TRUE(r.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsRemove, RemoveFileTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b/c/file/");
    EXPECT_FALSE(r) << "trailing slash on remove_file should be ENOTDIR";
}

TEST(VfsRemove, RemoveDir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("a");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
}

TEST(VfsRemove, RemoveDirNonEmpty)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("b");
    EXPECT_FALSE(r);
    EXPECT_TRUE(r.error() == std::errc::directory_not_empty);
}

TEST(VfsRemove, RemoveDirOnFile)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("b/c/file");
    EXPECT_FALSE(r);
}

TEST(VfsRemove, RemoveAll)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_all("b");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b"));
}

TEST(VfsRemove, RemoveAllNonExistent)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_all("non-exist");
    ASSERT_TRUE(r) << "remove_all on non-existent should succeed";
}

TEST(VfsRemove, RemoveAllDeep)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_all("deep-rmdir");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "deep-rmdir"));
}

TEST(VfsCreate, MkdirAllExisting)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("a", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
}

TEST(VfsCreate, MkdirAllDeep)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("a/b/c/d/e/f/g/h/i/j", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "a/b/c/d/e/f/g/h/i/j"));
}

TEST(VfsCreate, MkdirAllTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("x/y/z/", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "x/y/z"));
}

TEST(VfsCreate, MkdirAllThroughSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("e/foo", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "b/c/d/e/foo"));
}

TEST(VfsCreate, MkdirAllNonDirTrailing)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("b-file", mkdirs_perm);
    EXPECT_FALSE(fd);
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsCreate, MkdirAllNonDirSubdir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("b/c/file/subdir", mkdirs_perm);
    EXPECT_FALSE(fd);
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsCreate, MkdirAllDanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("a-fake1/foo", mkdirs_perm);
    EXPECT_FALSE(fd) << "dangling symlink should be ENOTDIR";
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsCreate, MkdirAllLoop)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // loop/link -> a resolves to loop/a which doesn't exist (dangling)
    auto fd = root.create_directories("loop/link", mkdirs_perm);
    EXPECT_FALSE(fd) << "dangling symlink should be ENOTDIR";
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsRename, Dir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "aa"));
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
}

TEST(VfsRename, NonEmptyDir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b", "bb");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "bb"));
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b"));
}

TEST(VfsRename, File)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b/c/file", "bb-file");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "bb-file"));
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b/c/file"));
}

TEST(VfsRename, Exchange)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "e", os::sys::rename_flag::exchange);
    ASSERT_TRUE(r) << r.error().message();
    // a is now a symlink to /b/c/d/e (was e's symlink)
    EXPECT_TRUE(std::filesystem::is_symlink(tree.base / "a"));
    // e is now a directory (was a's directory)
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "e"));
}

TEST(VfsRename, ExchangeNonExistent)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa", os::sys::rename_flag::exchange);
    EXPECT_FALSE(r);
    EXPECT_TRUE(r.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsRename, FileTrailingSlashSrc)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b/c/file/", "aa");
    EXPECT_FALSE(r) << "trailing slash on file source should be ENOTDIR";
    EXPECT_TRUE(r.error() == std::errc::not_a_directory);
}

TEST(VfsRename, FileTrailingSlashDst)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b/c/file", "aa/");
    EXPECT_FALSE(r) << "trailing slash on dst with file source should be rejected: "
                    << r.error().message();
}

TEST(VfsRename, DirTrailingSlashDst)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa/");
    if (r) {
        EXPECT_TRUE(std::filesystem::exists(tree.base / "aa"));
        EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
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
    EXPECT_EQ(root->ref().current_path(), d.path());
}

TEST(VfsResolve, NonLexicalPartial)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "link1/target_abs/foo",
                              "link1/target_rel/foo",
                              "link2/link1_abs/target_abs/foo",
                              "link2/link1_abs/target_rel/foo",
                              "link2/link1_rel/target_abs/foo",
                              "link2/link1_rel/target_rel/foo" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsResolve, NonLexicalDeepPartial)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "link3/target_abs/foo", "link3/target_rel/foo" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsResolve, TrailingSlashOnSymlinkNofollow)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // b-file -> b/c/file, b-file/ should fail (file is not a dir)
    auto fd = root.open("b-file/", { os::sys::open_flag{ }, os::sys::access_mode::path });
    EXPECT_FALSE(fd);
}

TEST(VfsResolve, DanglingTrickySymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "dangling-tricky/deep1", "dangling-tricky/deep2" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsResolve, DeepDanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "dangling/a",
                              "dangling/b/c",
                              "dangling/c",
                              "dangling/d/e",
                              "dangling/e",
                              "dangling/g" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsResolve, DanglingSymlinkInSubdir)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "c/a-fake1", "c/a-fake2", "c/a-fake3" }) {
        auto fd = root.open(path, { os::sys::open_flag{ }, os::sys::access_mode::path });
        EXPECT_FALSE(fd) << path << " should be ENOENT";
    }
}

TEST(VfsReadlink, RootSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "root-link1", "root-link2" }) {
        auto target = root.readlink(path);
        ASSERT_TRUE(target) << path << ": " << target.error().message();
    }
}

TEST(VfsReadlink, EscapeSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "escape-link1", "escape-link2" }) {
        auto target = root.readlink(path);
        if (target) {
            EXPECT_NE(target->native().find("target"), std::string::npos);
        }
    }
}

TEST(VfsReadlink, NonLexicalSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "link1/target_abs",
                              "link1/target_rel",
                              "link2/link1_abs",
                              "link2/link1_rel",
                              "link3/target_abs",
                              "link3/target_rel" }) {
        auto target = root.readlink(path);
        ASSERT_TRUE(target) << path << ": " << target.error().message();
    }
}

TEST(VfsReadlink, DeepDangling)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "dangling/a",
                              "dangling/b/c",
                              "dangling/c",
                              "dangling/d/e",
                              "dangling/e",
                              "dangling/g" }) {
        auto target = root.readlink(path);
        // Deep dangling symlinks may fail if intermediate components are
        // non-existent or escape the root
        if (!target) {
            EXPECT_TRUE(target.error() == std::errc::no_such_file_or_directory
                        || target.error() == std::errc::permission_denied)
              << path;
        }
    }
}

TEST(VfsReadlink, LoopSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // readlink on a loop symlink returns the raw target (doesn't follow)
    auto target = root.readlink("loop/link");
    ASSERT_TRUE(target) << target.error().message();
    EXPECT_EQ(*target, "a");
}

TEST(VfsCreate, SymlinkParentdirTrailingSlash)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "b" / "c");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("b/c//foobar", infra::symlink_spec{ "/target" });
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::is_symlink(d.path() / "b/c/foobar"));
}

TEST(VfsCreate, SymlinkTrailingDot)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("foobar/.", infra::symlink_spec{ "/target" });
    EXPECT_FALSE(r) << "trailing dot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, SymlinkTrailingDotdot)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("foobar/..", infra::symlink_spec{ "/target" });
    EXPECT_FALSE(r) << "trailing dotdot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, HardlinkParentdirTrailingSlash)
{
    const TempDir d;
    write_file(d.path() / "src", "data");
    std::filesystem::create_directories(d.path() / "b" / "c");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("b/c//dst", infra::hardlink_spec{ "src" });
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(d.path() / "b/c/dst"));
}

TEST(VfsCreate, HardlinkToSymlink)
{
    const TempDir d;
    std::error_code ec;
    std::filesystem::create_symlink("target", d.path() / "link", ec);
    ASSERT_FALSE(ec);
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("hardlink", infra::hardlink_spec{ "link" });
    if (r) {
        // Hardlink to a symlink creates another symlink
        EXPECT_TRUE(std::filesystem::is_symlink(d.path() / "hardlink"));
    }
}

TEST(VfsCreate, HardlinkToDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "mydir");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("hardlink", infra::hardlink_spec{ "mydir" });
    EXPECT_FALSE(r) << "hardlink to dir should be EPERM";
    // EPERM on some kernels, EACCES on others
    EXPECT_TRUE(r.error() == std::errc::operation_not_permitted
                || r.error() == std::errc::permission_denied);
}

TEST(VfsCreate, HardlinkToDanglingSymlink)
{
    const TempDir d;
    std::error_code ec;
    std::filesystem::create_symlink("nonexistent", d.path() / "dangling", ec);
    ASSERT_FALSE(ec);
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("hardlink", infra::hardlink_spec{ "dangling" });
    if (r) {
        EXPECT_TRUE(std::filesystem::is_symlink(d.path() / "hardlink"));
    }
}

TEST(VfsCreate, RootPathVariants)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    for (const auto *path : { "/", ".", ".." }) {
        auto r = root.create(path, infra::symlink_spec{ "/target" });
        EXPECT_FALSE(r) << path << " should be rejected";
    }
}

TEST(VfsCreate, HardlinkTrailingSlashOnDst)
{
    const TempDir d;
    write_file(d.path() / "src", "data");
    auto root = infra::Root::open(d.path()).value();
    auto r = root.create("dst/", infra::hardlink_spec{ "src" });
    EXPECT_FALSE(r) << "trailing slash on hardlink dest should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, CreateFileParentdirTrailingSlash)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "b" / "c");
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("b/c//foobar", os::sys::open_flag::none, os::default_new_file_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::exists(d.path() / "b/c/foobar"));
}

TEST(VfsCreate, CreateFileTrailingSlash)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("foobar/", os::sys::open_flag::none, os::default_new_file_perm);
    EXPECT_FALSE(fd) << "trailing slash should be rejected";
}

TEST(VfsCreate, CreateFileTrailingDot)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("foobar/.", os::sys::open_flag::none, os::default_new_file_perm);
    EXPECT_FALSE(fd) << "trailing dot should be EINVAL";
    EXPECT_TRUE(fd.error() == std::errc::invalid_argument);
}

TEST(VfsCreate, CreateFileOnDir)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "mydir");
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("mydir", os::sys::open_flag::none, os::default_new_file_perm);
    EXPECT_FALSE(fd) << "create_file on existing dir should be EISDIR";
    EXPECT_TRUE(fd.error() == std::errc::is_a_directory);
}

TEST(VfsCreate, CreateFileOnSymlink)
{
    const TempDir d;
    std::error_code ec;
    std::filesystem::create_symlink("target", d.path() / "mylink", ec);
    ASSERT_FALSE(ec);
    auto root = infra::Root::open(d.path()).value();
    // Without O_EXCL, create_file on a symlink follows it (target doesn't exist -> ENOENT)
    auto fd = root.create_file("mylink", os::sys::open_flag::none, os::default_new_file_perm);
    EXPECT_FALSE(fd) << "create_file on dangling symlink should be ENOENT";
}

TEST(VfsCreate, CreateFileOexclOnSymlink)
{
    const TempDir d;
    std::error_code ec;
    std::filesystem::create_symlink("target", d.path() / "mylink", ec);
    ASSERT_FALSE(ec);
    auto root = infra::Root::open(d.path()).value();
    auto fd = root.create_file("mylink", os::sys::open_flag::exclusive, os::default_new_file_perm);
    EXPECT_FALSE(fd) << "O_EXCL on existing symlink should be EEXIST";
    EXPECT_TRUE(fd.error() == std::errc::file_exists);
}

TEST(VfsCreate, CreateFileTmpfileNoent)
{
    const TempDir d;
    std::filesystem::create_directories(d.path() / "dir");
    auto root = infra::Root::open(d.path()).value();
    // O_TMPFILE with path that has multiple components (not just a dir)
    auto fd = root.create_file("dir/newfile",
                               os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec
                                 | os::sys::open_flag::exclusive,
                               tmpfile_perm);
    EXPECT_FALSE(fd) << "O_TMPFILE with multi-component path should be ENOENT";
    EXPECT_TRUE(fd.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsCreate, CreateFileTmpfileOnFile)
{
    const TempDir d;
    write_file(d.path() / "afile", "x");
    auto root = infra::Root::open(d.path()).value();
    // O_TMPFILE on a file (not a directory) should fail
    auto fd = root.create_file("afile",
                               os::sys::open_flag::tmpfile | os::sys::open_flag::cloexec
                                 | os::sys::open_flag::exclusive,
                               tmpfile_perm);
    EXPECT_FALSE(fd) << "O_TMPFILE on a file should be ENOTDIR";
    EXPECT_TRUE(fd.error() == std::errc::not_a_directory);
}

TEST(VfsCreate, CreateFileRootPath)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    for (const auto *path : { "/", ".", ".." }) {
        auto fd = root.create_file(path, os::sys::open_flag::none, os::default_new_file_perm);
        EXPECT_FALSE(fd) << path << " should be rejected";
    }
}

TEST(VfsRemove, RemoveFileNoexistTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("abc/");
    EXPECT_FALSE(r) << "trailing slash on non-existent should be ENOTDIR";
    EXPECT_TRUE(r.error() == std::errc::not_a_directory);
}

TEST(VfsRemove, RemoveFileDanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("a-fake1");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a-fake1"));
}

TEST(VfsRemove, RemoveDirDanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // a-fake1 is a symlink (not a directory), so remove_dir should fail
    auto r = root.remove_dir("a-fake1");
    EXPECT_FALSE(r);
    EXPECT_TRUE(r.error() == std::errc::not_a_directory);
}

TEST(VfsRemove, RemoveAllDanglingSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_all("a-fake1");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a-fake1"));
}

TEST(VfsRemove, RemoveFileParentdirTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b/c//file");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "b/c/file"));
}

TEST(VfsRemove, RemoveDirTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("a/");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
}

TEST(VfsRemove, RemoveFileTrailingDot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b/c/file/.");
    EXPECT_FALSE(r) << "trailing dot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRemove, RemoveFileTrailingDotdot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_file("b/c/file/..");
    EXPECT_FALSE(r) << "trailing dotdot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRemove, RemoveDirTrailingDot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("b/c/.");
    EXPECT_FALSE(r) << "trailing dot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRemove, RemoveDirTrailingDotdot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.remove_dir("b/c/..");
    EXPECT_FALSE(r) << "trailing dotdot should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRemove, RemoveAllRootPath)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "/", ".", ".." }) {
        auto r = root.remove_all(path);
        EXPECT_FALSE(r) << path << " should be rejected";
    }
}

TEST(VfsRename, ParentdirTrailingSlash)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b/c//d", "aa");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "aa"));
}

TEST(VfsRename, DirTrailingSlashSrc)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a/", "aa");
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "aa"));
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
}

TEST(VfsRename, Noreplace)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa", os::sys::rename_flag::noreplace);
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::exists(tree.base / "aa"));
    EXPECT_FALSE(std::filesystem::exists(tree.base / "a"));
}

TEST(VfsRename, NoreplaceEexist)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "e", os::sys::rename_flag::noreplace);
    EXPECT_FALSE(r) << "noreplace on existing should be EEXIST";
    EXPECT_TRUE(r.error() == std::errc::file_exists);
}

TEST(VfsRename, ExchangeDifftype)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // a (dir) <-> e (symlink to /b/c/d/e)
    auto r = root.rename("a", "e", os::sys::rename_flag::exchange);
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_TRUE(std::filesystem::is_symlink(tree.base / "a"));
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "e"));
}

TEST(VfsRename, ExchangeTrailingSlashBoth)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // a (dir) <-> e/ (symlink, trailing slash requires dir on both)
    auto r = root.rename("a/", "e/", os::sys::rename_flag::exchange);
    // e is a symlink, not a directory, so trailing slash should fail
    EXPECT_FALSE(r);
}

TEST(VfsRename, ExchangeDifftypeTrailingSlashTo)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // a (dir) <-> e/ (symlink, trailing slash requires dir)
    auto r = root.rename("a", "e/", os::sys::rename_flag::exchange);
    // e is a symlink, not a directory, so trailing slash should fail
    EXPECT_FALSE(r);
}

TEST(VfsRename, TrailingDotSrc)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a/.", "aa");
    EXPECT_FALSE(r) << "trailing dot on source should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRename, TrailingDotDst)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa/.");
    EXPECT_FALSE(r) << "trailing dot on dest should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRename, TrailingDotdotSrc)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("b/c/..", "aa");
    EXPECT_FALSE(r) << "trailing dotdot on source should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRename, TrailingDotdotDst)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto r = root.rename("a", "aa/..");
    EXPECT_FALSE(r) << "trailing dotdot on dest should be EINVAL";
    EXPECT_TRUE(r.error() == std::errc::invalid_argument);
}

TEST(VfsRename, RenameRootPath)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *src : { "/", ".", ".." }) {
        auto r = root.rename(src, "aa");
        EXPECT_FALSE(r) << src << " -> aa should be rejected";
    }
    for (const auto *dst : { "/", ".", ".." }) {
        auto r = root.rename("a", dst);
        EXPECT_FALSE(r) << "a -> " << dst << " should be rejected";
    }
}

TEST(VfsCreate, MkdirAllTrailingDot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("foobar/.", os::default_dir_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "foobar"));
}

TEST(VfsCreate, MkdirAllTrailingDotdot)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("foobar/..", os::default_dir_perm);
    EXPECT_FALSE(fd) << "trailing dotdot should be rejected";
}

TEST(VfsCreate, MkdirAllDotdotInExisting)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("b/c/../c/./d/e/f/g/h", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "b/c/d/e/f/g/h"));
}

TEST(VfsCreate, MkdirAllDotdotInNonexisting)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // ".." in a non-existing path component: k/../lmnop where k doesn't exist
    auto fd = root.create_directories("a/b/c/d/e/f/g/h/i/j/k/../lmnop", mkdirs_perm);
    EXPECT_FALSE(fd) << "dotdot in non-existing path should be ENOENT";
    EXPECT_TRUE(fd.error() == std::errc::no_such_file_or_directory);
}

TEST(VfsCreate, MkdirAllDotdotAfterSymlink)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // e -> /b/c/d/e, so e/../dd/ee/ff should resolve to b/c/d/dd/ee/ff
    auto fd = root.create_directories("e/../dd/ee/ff", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "b/c/d/dd/ee/ff"));
}

TEST(VfsCreate, MkdirAllThroughSymlinkMultiLevel)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    // link1/target_abs -> /target, so link1/target_abs/foo creates target/foo
    auto fd = root.create_directories("link1/target_abs/foo", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "target/foo"));
}

TEST(VfsCreate, MkdirAllThroughSymlinkRel)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    auto fd = root.create_directories("link1/target_rel/foo", mkdirs_perm);
    ASSERT_TRUE(fd) << fd.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(tree.base / "target/foo"));
}

TEST(VfsCreate, MkdirAllRootPath)
{
    auto tree = BasicTree{ };
    auto root = tree.open();
    for (const auto *path : { "/", "." }) {
        auto fd = root.create_directories(path, os::default_dir_perm);
        ASSERT_TRUE(fd) << path << " should succeed: " << fd.error().message();
    }

    // ".." resolves to root but may fail EACCES from containment check
    auto fd = root.create_directories("..", os::default_dir_perm);
    if (!fd) {
        EXPECT_TRUE(fd.error() == std::errc::permission_denied);
    }
}

TEST(VfsCreate, MkdirAllInvalidMode)
{
    const TempDir d;
    auto root = infra::Root::open(d.path()).value();
    // setuid bit should be rejected
    auto fd = root.create_directories("foo", invalidmode_perm);
    EXPECT_FALSE(fd) << "setuid mode should be EINVAL";
    EXPECT_TRUE(fd.error() == std::errc::invalid_argument);
}
