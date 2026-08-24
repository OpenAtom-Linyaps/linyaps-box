// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/status_directory.h"

#include "linyaps_box/io/stream.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

namespace {

void atomic_write(const std::filesystem::path &path, std::string_view content)
{
    using namespace linyaps_box::os;
    using namespace linyaps_box::utils;

    auto dir = path.parent_path();
    auto dirfd =
      throw_if_error(open(dir,
                          sys::open_option{ sys::open_flag::directory | sys::open_flag::cloexec,
                                            sys::access_mode::read_only }));

    int max_retries{ 7 };

    std::string temp_name(".tmp-XXXXXX");
    file_descriptor fd;
    for (; max_retries >= 0; --max_retries) {
        temp_name.replace(5, 6, gen_random_string(6));
        auto temp_path = dir / temp_name;
        auto temp = open(temp_path,
                         sys::open_option{ sys::open_flag::create | sys::open_flag::exclusive
                                             | sys::open_flag::cloexec | sys::open_flag::no_follow,
                                           sys::access_mode::read_write },
                         std::filesystem::perms::owner_all);
        if (LIKELY(temp.has_value())) {
            fd = std::move(*temp);
            break;
        }

        const auto &err = temp.error();
        if (err == std::errc::file_exists) {
            continue;
        }

        throw std::system_error(err, "failed to create temporary status file");
    }

    if (max_retries < 0) {
        throw std::runtime_error("maximum number of attempts to create a temporary file reached");
    }

    auto cleanup = make_errdefer([&dirfd, &temp_name]() noexcept {
        std::ignore = unlinkat(dirfd, temp_name);
    });

    throw_if_error(linyaps_box::io::write_all(fd, as_bytes(span(content))));

    if (::fsync(fd.get()) != 0) {
        LINYAPS_BOX_LOG_WARN("fsync failed for status file: {}",
                             std::generic_category().message(errno));
    }

    auto ref = dirfd.ref();
    throw_if_error(renameat2(ref, temp_name, ref, path.filename(), sys::rename_flag::none));

    if (::fsync(ref) != 0) {
        LINYAPS_BOX_LOG_WARN("fsync failed for status directory: {}",
                             std::generic_category().message(errno));
    }
}

auto read_status(const std::filesystem::path &path) -> linyaps_box::container_status
{
    using namespace linyaps_box::os;

    auto fd = throw_if_error(
      open(path, sys::open_option{ sys::open_flag::cloexec, sys::access_mode::read_only }));

    linyaps_box::utils::uninit_vector<std::byte> buf;
    std::ignore = throw_if_error(linyaps_box::io::read_to_end(fd, buf));

    const auto j = nlohmann::json::parse(buf.cbegin(), buf.cend());
    return j.get<linyaps_box::container_status>();
}

} // namespace

linyaps_box::status_directory::status_directory(std::filesystem::path path)
    : path_(std::move(path))
{
    if (std::filesystem::is_directory(path_) || std::filesystem::create_directories(path_)) {
        return;
    }

    throw std::runtime_error("failed to create status directory: " + path_.string());
}

void linyaps_box::status_directory::write(const container_status &status) const
{
    auto j = nlohmann::json(status);
    ::atomic_write(path_ / "status.json", j.dump());
}

auto linyaps_box::status_directory::read() const -> container_status
{
    return read_status(path_ / "status.json");
}

void linyaps_box::status_directory::remove() const
{
    LINYAPS_BOX_LOG_DEBUG("Remove {}", path_);
    std::filesystem::remove_all(path_);
}

auto linyaps_box::status_directory::write_config(std::string_view config) const -> void
{
    ::atomic_write(path_ / "config.json", config);
}

auto linyaps_box::status_directory::save_config(const std::filesystem::path &src) const -> void
{
    std::filesystem::copy(src, path_ / "config.json");
}

auto linyaps_box::status_directory::config() const -> std::filesystem::path
{
    return path_ / "config.json";
}
