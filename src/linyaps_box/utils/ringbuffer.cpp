// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/ringbuffer.h"

namespace linyaps_box::utils {

ring_buffer_deleter::ring_buffer_deleter(std::size_t total_size) noexcept
    : total_size{ total_size }
{
}

auto ring_buffer_deleter::operator()(ring_buffer *rb) const -> void
{
    rb->~ring_buffer();
    ::operator delete(rb,
                      total_size,
                      std::align_val_t(compat::hardware_constructive_interference_size));
}

auto ring_buffer::create(std::size_t requested_capacity)
        -> std::unique_ptr<ring_buffer, ring_buffer_deleter>
{
    requested_capacity = std::max<std::size_t>(requested_capacity, 2);

    std::size_t capacity = 1;
    while (capacity < requested_capacity) {
        capacity <<= 1U;
    }

    const auto total_size = sizeof(ring_buffer) + capacity;
    auto *mem = ::operator new(total_size,
                               std::align_val_t(compat::hardware_constructive_interference_size));
    auto *rb = new (mem) ring_buffer(capacity);
    return { rb, ring_buffer_deleter(total_size) };
}

auto ring_buffer::get_read_vecs() const noexcept -> iov_view
{
    if (empty()) {
        return {};
    }

    const auto *base = data_ptr();
    if (tail_ > head_) {
        return { { { const_cast<std::byte *>(base + head_), tail_ - head_ }, // NOLINT
                   { nullptr, 0 } } };
    }

    return { { { const_cast<std::byte *>(base + head_), capacity_ - head_ }, // NOLINT
               { const_cast<std::byte *>(base), tail_ } } };                 // NOLINT
}

auto ring_buffer::get_write_vecs() const noexcept -> iov_view
{
    if (full()) {
        return {};
    }

    const auto *base = data_ptr();
    const auto space = capacity_ - size() - 1;

    if (tail_ >= head_) {
        const auto first_part = std::min(space, capacity_ - tail_);
        const auto second_part = space - first_part;
        return { { { const_cast<std::byte *>(base + tail_), first_part }, // NOLINT
                   { const_cast<std::byte *>(base), second_part } } };    // NOLINT
    }

    return { { { const_cast<std::byte *>(base + tail_), space }, { nullptr, 0 } } }; // NOLINT
}

} // namespace linyaps_box::utils
