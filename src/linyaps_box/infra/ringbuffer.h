// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/span.h"

#include <cassert>
#include <cstddef>
#include <memory>

namespace linyaps_box::infra {

// Single-threaded, single-owner only ring buffer.
class ring_buffer
{
public:
    struct deleter
    {
        auto operator()(ring_buffer *rb) const noexcept -> void;
    };

    using ptr = std::unique_ptr<ring_buffer, deleter>;
    static_assert(sizeof(ptr) == sizeof(void *), "ring_buffer::ptr must stay one pointer");

    ring_buffer(const ring_buffer &) = delete;
    ring_buffer &operator=(const ring_buffer &) = delete;
    ring_buffer(ring_buffer &&) = delete;
    ring_buffer &operator=(ring_buffer &&) = delete;
    ~ring_buffer() = default;

    static auto create(std::size_t requested_capacity) -> ptr;

    [[nodiscard]] auto get_read_ptr() noexcept -> std::byte *
    {
        return data_ptr_ + (tail_ & mask_);
    }

    [[nodiscard]] auto get_write_ptr() noexcept -> std::byte *
    {
        return data_ptr_ + (head_ & mask_);
    }

    [[nodiscard]] auto readable() const noexcept -> utils::span<const std::byte>
    {
        return { data_ptr_ + (tail_ & mask_), head_ - tail_ };
    }

    [[nodiscard]] auto writable() noexcept -> utils::span<std::byte>
    {
        return { data_ptr_ + (head_ & mask_), (mask_ + 1) - (head_ - tail_) };
    }

    auto advance_head(std::size_t n) noexcept -> void
    {
        assert(n <= free_space());
        head_ += n;
    }

    auto advance_tail(std::size_t n) noexcept -> void
    {
        assert(n <= size());
        tail_ += n;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return head_ - tail_; }

    [[nodiscard]] auto free_space() const noexcept -> std::size_t
    {
        return (mask_ + 1) - (head_ - tail_);
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return head_ == tail_; }

    [[nodiscard]] auto full() const noexcept -> bool { return (head_ - tail_) == (mask_ + 1); }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t { return mask_ + 1; }

    auto clear() noexcept -> void
    {
        head_ = 0;
        tail_ = 0;
    }

private:
    explicit ring_buffer(std::size_t cap, std::byte *data_base)
        : data_ptr_(data_base)
        , mask_(cap - 1)
    {
    }

    std::byte *data_ptr_;
    std::size_t head_{ 0 };
    std::size_t tail_{ 0 };
    std::size_t mask_;
};

} // namespace linyaps_box::infra
