// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/stream.h"

#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/io.h"
#include "linyaps_box/utils/utils.h"

#include <algorithm>
#include <cstring>

namespace linyaps_box::io {

namespace {

constexpr std::size_t probe_size = 32;
constexpr std::size_t default_buf_size = 8192;

auto detect_file_size_hint(utils::file_descriptor_ref fd) noexcept -> std::size_t
{
    auto stat = os::fstat(fd);
    if (!stat) {
        return 0;
    }

    auto type = os::to_fs_file_type(stat->st_mode);
    if (type == std::filesystem::file_type::regular) {
        const auto current_pos = ::lseek(fd, 0, SEEK_CUR);
        if (current_pos != -1 && stat->st_size > current_pos) {
            return stat->st_size - current_pos;
        }
    }

    return 0;
}

auto probe_read(utils::file_descriptor_ref fd, utils::uninit_vector<std::byte> &buf) noexcept
  -> os::Result<std::size_t>
{
    std::array<std::byte, probe_size> probe; // NOLINT
    auto n = os::read(fd, probe);
    if (UNLIKELY(!n)) {
        return os::unexpected{ n.error() };
    }

    const auto bytes_read = *n;
    if (bytes_read > 0) {
        const auto old_size = buf.size();
        buf.resize(old_size + bytes_read);
        std::memcpy(buf.data() + old_size, probe.data(), bytes_read);
    }

    return bytes_read;
}
} // namespace

auto read_to_end(utils::file_descriptor_ref fd, utils::uninit_vector<std::byte> &buf) noexcept
  -> os::Result<std::size_t>
{
    const auto start_len = buf.size();
    const auto size_hint = detect_file_size_hint(fd);
    if (size_hint > 0) {
        buf.reserve(buf.size() + size_hint + 1);
    } else if (buf.capacity() - buf.size() < default_buf_size) {
        buf.reserve(buf.size() + default_buf_size);
    }

    const auto start_cap = buf.capacity();
    if (start_cap - buf.size() < probe_size) {
        auto read_res = probe_read(fd, buf);
        if (UNLIKELY(!read_res)) {
            return os::unexpected{ read_res.error() };
        }

        if (*read_res == 0) {
            return 0; // EOF
        }
    }

    std::size_t max_read_size =
      (size_hint > 0) ? std::max(size_hint, default_buf_size) : default_buf_size;

    while (true) {
        if (buf.size() == buf.capacity()) {
            const auto current_cap = buf.capacity();
            const auto growth = std::max(current_cap, default_buf_size);
            buf.reserve(current_cap + growth);
        }

        const auto old_size = buf.size();
        const auto spare_capacity = buf.capacity() - old_size;
        const auto to_read = std::min(spare_capacity, max_read_size);

        const utils::span spare_space{ buf.data() + old_size, to_read };

        auto n = os::read(fd, spare_space);
        if (UNLIKELY(!n)) {
            return os::unexpected{ n.error() };
        }

        const auto bytes_read = *n;
        if (bytes_read == 0) {
            break;
        }

        buf.resize(old_size + bytes_read);

        if (size_hint == 0 && to_read >= max_read_size && bytes_read == to_read) {
            max_read_size = std::min(max_read_size * 2, static_cast<std::size_t>(1024 * 1024));
        }
    }

    return buf.size() - start_len;
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
