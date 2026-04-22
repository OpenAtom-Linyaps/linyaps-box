// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <memory>

namespace linyaps_box::compat {
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
// default to 64 bytes if not defined
// if the platform has different cache line size, please define these macros accordingly
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif
} // namespace linyaps_box::compat

namespace linyaps_box::utils {

class alignas(compat::hardware_constructive_interference_size) ring_buffer
{
public:
    struct deleter
    {
        auto operator()(ring_buffer *rb) const noexcept -> void;
        std::size_t total_size;
    };

    using ptr = std::unique_ptr<ring_buffer, deleter>;

    ring_buffer(const ring_buffer &) = delete;
    ring_buffer &operator=(const ring_buffer &) = delete;
    ring_buffer(ring_buffer &&) = delete;
    ring_buffer &operator=(ring_buffer &&) = delete;
    ~ring_buffer() = default;

    static auto create(std::size_t requested_capacity) -> ptr;

    [[nodiscard]] auto get_read_ptr() const noexcept -> const std::byte *
    {
        return data_ptr_ + (tail_ & mask_);
    }

    [[nodiscard]] auto get_read_ptr() noexcept -> std::byte *
    {
        return data_ptr_ + (tail_ & mask_);
    }

    [[nodiscard]] auto get_write_ptr() const noexcept -> const std::byte *
    {
        return data_ptr_ + (head_ & mask_);
    }

    [[nodiscard]] auto get_write_ptr() noexcept -> std::byte *
    {
        return data_ptr_ + (head_ & mask_);
    }

    auto advance_head(std::size_t n) noexcept -> void { head_ += n; }

    auto advance_tail(std::size_t n) noexcept -> void { tail_ += n; }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return head_ - tail_; }

    [[nodiscard]] auto free_space() const noexcept -> std::size_t
    {
        return (mask_ + 1) - (head_ - tail_);
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return head_ == tail_; }

    [[nodiscard]] auto full() const noexcept -> bool { return (head_ - tail_) == (mask_ + 1); }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t { return mask_ + 1; }

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

} // namespace linyaps_box::utils
