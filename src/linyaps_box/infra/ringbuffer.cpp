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
} // namespace

auto ring_buffer::deleter::operator()(ring_buffer *rb) const noexcept -> void
{
    if (rb == nullptr) {
        return;
    }

    rb->~ring_buffer();

    auto ret = os::munmap(rb, total_size);
    if (UNLIKELY(!ret)) {
        // Memory leak, but will be cleaned up by OS on process exit
        LINYAPS_BOX_LOG_WARN("Failed to munmap ring buffer: {}", ret.error());
    }
}

auto ring_buffer::create(std::size_t requested_capacity) -> ptr
{
    const auto page_size = get_page_size();
    const std::size_t meta_size = (sizeof(ring_buffer) + page_size - 1) & ~(page_size - 1);

    auto cap = page_size;
    constexpr auto max_cap = (std::numeric_limits<std::size_t>::max() / 4);
    while (cap < requested_capacity) {
        if (UNLIKELY(cap > max_cap)) {
            throw std::runtime_error("requested ring buffer capacity too large");
        }

        cap <<= 1U;
    }

    auto total_vma = meta_size + (2 * cap);

    auto fd =
      os::throw_if_error(os::memfd_create("linyaps_box_io_buffer", os::sys::memfd_flag::cloexec),
                         "failed to create memfd");

    os::throw_if_error(os::ftruncate(fd.ref(), static_cast<off_t>(meta_size + cap)),
                       "failed to truncate memfd");

    auto *addr = os::throw_if_error(
      os::mmap_anonymous(nullptr, total_vma, os::sys::prot_flag::none, os::sys::map_flag::private_),
      "failed to mmap memory");

    auto mem_fail_guard = utils::make_errdefer([addr, total_vma]() noexcept {
        auto ret = os::munmap(addr, total_vma);
        if (UNLIKELY(!ret)) {
            LINYAPS_BOX_LOG_WARN("Failed to munmap ring buffer after mmap failure: {}",
                                 ret.error());
        }
    });

    constexpr auto prot = os::sys::prot_flag::read | os::sys::prot_flag::write;
    constexpr auto map =
      os::sys::map_flag::shared | os::sys::map_flag::fixed | os::sys::map_flag::populate;

    os::throw_if_error(os::mmap(addr, meta_size + cap, prot, map, fd, 0),
                       "failed to mmap ring buffer");

    auto *mirror_addr = static_cast<std::byte *>(addr) + meta_size + cap;
    os::throw_if_error(os::mmap(mirror_addr, cap, prot, map, fd, static_cast<off_t>(meta_size)),
                       "failed to mmap ring buffer");

    auto *data_base = static_cast<std::byte *>(addr) + meta_size;
    auto *rb = new (addr) ring_buffer(cap, data_base);

    auto deleter = ring_buffer::deleter{ total_vma };
    return { rb, std::move(deleter) };
}

} // namespace linyaps_box::infra
