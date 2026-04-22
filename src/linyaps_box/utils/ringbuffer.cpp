// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/ringbuffer.h"

#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/mman.h"
#include "linyaps_box/utils/platform.h"

#include <cassert>
#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace linyaps_box::utils {

auto ring_buffer::deleter::operator()(ring_buffer *rb) const noexcept -> void
{
    if (rb == nullptr) {
        return;
    }

    rb->~ring_buffer();

    try {
        munmap(reinterpret_cast<std::byte *>(rb), total_size); // NOLINT
    } catch (const std::system_error &e) {
        LINYAPS_BOX_ERR() << "Failed to munmap ring buffer: " << e.what();
        assert(false);
    }
}

auto ring_buffer::create(std::size_t requested_capacity) -> ptr
{
    const auto page_size = utils::get_page_size();
    auto meta_size = (sizeof(ring_buffer) + page_size - 1) & ~(page_size - 1);

    auto cap = page_size;
    while (cap < requested_capacity) {
        cap <<= 1U;
    }

    auto total_vma = meta_size + (2 * cap);

    auto fd = memfd_create("linyaps_box_io_buffer", MFD_CLOEXEC);
    if (::ftruncate(fd.get(), meta_size + cap) == -1) {
        throw std::system_error(errno, std::system_category(), "ftruncate failed");
    }

    auto *addr = mmap(nullptr, total_vma, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, std::nullopt, 0);
    auto mem_guard = utils::make_errdefer([addr, total_vma]() noexcept {
        munmap(addr, total_vma);
    });

    mmap(addr,
         meta_size + cap,
         PROT_READ | PROT_WRITE,
         MAP_SHARED | MAP_FIXED | MAP_POPULATE,
         fd,
         0);

    auto *mirror_addr = static_cast<std::byte *>(addr) + meta_size + cap;
    mmap(mirror_addr, cap, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, meta_size);

    auto *data_base = static_cast<std::byte *>(addr) + meta_size;
    auto *rb = new (addr) ring_buffer(cap, data_base);

    auto deleter = ring_buffer::deleter{ total_vma };
    return { rb, std::move(deleter) };
}

} // namespace linyaps_box::utils
