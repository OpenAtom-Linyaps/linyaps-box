// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/stream.h"

#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/io.h"
#include "linyaps_box/utils/utils.h"

#include <algorithm>

namespace linyaps_box::io {

namespace {

constexpr std::size_t default_buf_size = 8192;

// Sizes the first read when the caller has no hint: a regular file's st_size
// minus whatever the descriptor has already consumed.  /proc files report
// st_size 0 and fall through to default_buf_size, which is why the read loop
// still has to grow.
auto detect_file_size_hint(utils::file_descriptor_ref fd) noexcept -> std::size_t
{
    auto stat = os::fstat(fd);
    if (!stat) {
        return 0;
    }

    const auto type = os::to_fs_file_type(stat->st_mode);
    if (type == std::filesystem::file_type::regular) {
        const auto current_pos = ::lseek(fd, 0, SEEK_CUR);
        if (current_pos != -1 && stat->st_size > current_pos) {
            return stat->st_size - current_pos;
        }
    }

    return 0;
}

} // namespace

auto read_all(utils::file_descriptor_ref fd, std::size_t hint) noexcept -> os::Result<std::string>
{
    const auto size_hint = hint != 0 ? hint : detect_file_size_hint(fd);

    // A std::string cannot commit uninitialised storage (C++17), so the expected
    // size is sized up front, the reads fill that tail and `used` tracks what was
    // actually written; the tail is truncated before returning.
    // TODO(C++23): resize_and_overwrite would let the reads fill uninitialised
    // storage instead of a value-initialised tail.
    std::string out;
    out.resize(size_hint > 0 ? size_hint + 1 : default_buf_size);

    std::size_t used{ 0 };
    while (true) {
        if (UNLIKELY(used == out.size())) {
            out.resize(out.size() + std::max(out.size(), default_buf_size));
        }

        const auto want = out.size() - used;
        auto n = os::read(fd, utils::as_writable_bytes(utils::span{ out.data() + used, want }));
        if (UNLIKELY(!n)) {
            return os::unexpected{ n.error() };
        }

        if (*n == 0) {
            break;
        }

        used += *n;
    }

    out.resize(used);
    return out;
}

auto read_all(const std::filesystem::path &path) noexcept -> os::Result<std::string>
{
    auto fd = os::open(path, { os::sys::open_flag::cloexec, os::sys::access_mode::read_only });
    if (UNLIKELY(!fd)) {
        return os::unexpected{ fd.error() };
    }

    const auto stat = os::fstat(*fd);
    if (UNLIKELY(!stat)) {
        return os::unexpected{ stat.error() };
    }

    if (UNLIKELY(os::to_fs_file_type(stat->st_mode) != std::filesystem::file_type::regular)) {
        return os::unexpected{ std::make_error_code(std::errc::invalid_argument) };
    }

    return read_all(*fd, static_cast<std::size_t>(stat->st_size));
}

auto write_all(utils::file_descriptor_ref fd, utils::span<const std::byte> buf) noexcept
  -> os::Result<void>
{
    while (!buf.empty()) {
        auto ret = os::write(fd, buf);
        if (UNLIKELY(!ret)) {
            return os::unexpected{ ret.error() };
        }

        buf = buf.subspan(*ret);
    }

    return { };
}

} // namespace linyaps_box::io
