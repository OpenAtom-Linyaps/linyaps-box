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

constexpr auto probe_size = 32;
constexpr auto default_chunk_size = 8192;
constexpr auto max_chunk_size = 64UL * 1024;

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

auto probe_and_grow(utils::file_descriptor_ref fd, utils::uninit_vector<std::byte> &buf) noexcept
  -> os::Result<bool>
{
    std::array<std::byte, probe_size> probe; // NOLINT
    auto probe_n = os::read(fd, probe);
    if (UNLIKELY(!probe_n)) {
        return os::unexpected{ probe_n.error() };
    }

    if (*probe_n == 0) {
        return true; // EOF
    }

    if (*probe_n == probe_size) {
        buf.reserve(buf.size() + probe_size + default_chunk_size);
    }

    const auto old_size = buf.size();
    buf.resize(old_size + *probe_n);
    std::memcpy(buf.data() + old_size, probe.data(), *probe_n);

    return *probe_n < probe_size;
}
} // namespace

auto read_to_end(utils::file_descriptor_ref fd, utils::uninit_vector<std::byte> &buf) noexcept
  -> os::Result<std::size_t>
{
    const auto initial_size = buf.size();
    const auto initial_cap = buf.capacity();

    const auto size_hint = detect_file_size_hint(fd);
    if (size_hint > 0) {
        buf.reserve(buf.size() + size_hint + 1024);
    }

    if (size_hint == 0 && buf.capacity() - buf.size() < probe_size) {
        auto done = probe_and_grow(fd, buf);
        if (UNLIKELY(!done)) {
            return os::unexpected{ done.error() };
        }

        if (*done) {
            return buf.size() - initial_size;
        }
    }

    std::size_t chunk_size = default_chunk_size;

    while (true) {
        if (buf.size() == buf.capacity()) {
            if (buf.capacity() == initial_cap && initial_cap > 0) {
                auto done = probe_and_grow(fd, buf);
                if (UNLIKELY(!done)) {
                    return os::unexpected{ done.error() };
                }

                if (*done) {
                    break;
                }
            } else {
                buf.reserve(buf.size() + chunk_size);
            }
        }

        const auto old_size = buf.size();
        const auto available = buf.capacity() - old_size;
        const auto to_read = std::min(available, chunk_size);

        buf.resize(old_size + to_read);
        const utils::span available_space{ buf.data() + old_size, to_read };

        auto n = os::read(fd, available_space);
        if (UNLIKELY(!n)) {
            buf.resize(old_size);
            return os::unexpected{ n.error() };
        }

        if (*n == 0) {
            buf.resize(old_size);
            break;
        }

        if (*n < to_read) {
            buf.resize(old_size + *n);
        }

        if (*n == to_read && chunk_size < max_chunk_size) {
            chunk_size *= 2;
        }
    }

    return buf.size() - initial_size;
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
