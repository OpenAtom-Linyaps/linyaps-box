// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/infra/ringbuffer.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/mm.h"
#include "linyaps_box/utils/defer.h"

#include <fmt/std.h>

#include <unistd.h>

namespace linyaps_box::infra {

namespace {
auto get_page_size() noexcept -> std::size_t
{
    static const auto page_size = []() noexcept -> std::size_t {
        errno = 0;
        const auto sz = ::sysconf(_SC_PAGESIZE);

        if (sz == -1) {
            if (errno != 0) {
                LINYAPS_BOX_LOG_ERROR_ERRNO(errno, "Failed to get page size, defaulting to 4096");
            }

            return 4096;
        }

        return static_cast<std::size_t>(sz);
    }();

    return page_size;
}

auto meta_size() noexcept -> std::size_t
{
    const auto page_size = get_page_size();
    return (sizeof(ring_buffer) + page_size - 1) & ~(page_size - 1);
}
} // namespace

auto ring_buffer::deleter::operator()(ring_buffer *rb) const noexcept -> void
{
    const std::size_t cap = rb->capacity();
    rb->~ring_buffer();

    // ignore even if failed to unmap
    std::ignore = os::munmap(rb, meta_size() + (2 * cap));
}

auto ring_buffer::create(std::size_t requested_capacity) -> ptr
{
    const auto page_size = get_page_size();
    const std::size_t meta = meta_size();

    auto cap = page_size;
    constexpr auto max_cap = (std::numeric_limits<std::size_t>::max() / 4);
    while (cap < requested_capacity) {
        if (UNLIKELY(cap > max_cap)) {
            throw std::runtime_error("requested ring buffer capacity too large");
        }

        cap <<= 1U;
    }

    const auto total_vma = meta + (2 * cap);
    const auto fd =
      os::throw_if_error(os::memfd_create("linyaps_box_io_buffer", os::sys::memfd_flag::cloexec),
                         "failed to create memfd");

    os::throw_if_error(os::ftruncate(fd.ref(), static_cast<off_t>(meta + cap)),
                       "failed to truncate memfd");

    auto *addr = os::throw_if_error(
      os::mmap_anonymous(nullptr, total_vma, os::sys::prot_flag::none, os::sys::map_flag::private_),
      "failed to mmap memory");

    const auto mem_fail_guard = utils::make_errdefer([addr, total_vma]() noexcept {
        std::ignore = os::munmap(addr, total_vma);
    });

    constexpr auto prot = os::sys::prot_flag::read | os::sys::prot_flag::write;
    constexpr auto map =
      os::sys::map_flag::shared | os::sys::map_flag::fixed | os::sys::map_flag::populate;

    os::throw_if_error(os::mmap(addr, meta + cap, prot, map, fd, 0), "failed to mmap ring buffer");

    auto *mirror_addr = static_cast<std::byte *>(addr) + meta + cap;
    os::throw_if_error(os::mmap(mirror_addr, cap, prot, map, fd, static_cast<off_t>(meta)),
                       "failed to mmap ring buffer");

    auto *data_base = static_cast<std::byte *>(addr) + meta;
    auto *rb = new (addr) ring_buffer(cap, data_base);

    return ptr{ rb };
}

} // namespace linyaps_box::infra
