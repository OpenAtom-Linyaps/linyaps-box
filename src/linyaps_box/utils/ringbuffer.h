// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <new>

#include <sys/uio.h>
#include <unistd.h>

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

class ring_buffer;

struct ring_buffer_deleter
{
    explicit ring_buffer_deleter(std::size_t total_size) noexcept;

    auto operator()(ring_buffer *rb) const -> void;

private:
    std::size_t total_size{ 0 };
};

class alignas(compat::hardware_constructive_interference_size) ring_buffer
{
    using iov_view = std::array<struct iovec, 2>;

public:
    enum class Status : uint8_t { Success, TryAgain, Full, Empty, Finished };

    ring_buffer(const ring_buffer &) = delete;
    ring_buffer &operator=(const ring_buffer &) = delete;
    ring_buffer(ring_buffer &&) = default;
    ring_buffer &operator=(ring_buffer &&) = default;
    ~ring_buffer() = default;

    static auto create(std::size_t requested_capacity)
            -> std::unique_ptr<ring_buffer, ring_buffer_deleter>;

    [[nodiscard]] auto get_read_vecs() const noexcept -> iov_view;

    auto advance_head(std::size_t n) noexcept -> void { head_ = (head_ + n) & mask_; }

    [[nodiscard]] auto get_write_vecs() const noexcept -> iov_view;

    auto advance_tail(std::size_t n) noexcept -> void { tail_ = (tail_ + n) & mask_; }

    [[nodiscard]] auto empty() const noexcept -> bool { return head_ == tail_; }

    [[nodiscard]] auto full() const noexcept -> bool { return ((tail_ + 1) & mask_) == head_; }

    [[nodiscard]] auto capacity() const noexcept -> std::size_t { return capacity_ - 1; }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return (tail_ - head_) & mask_; }

private:
    explicit ring_buffer(std::size_t cap)
        : capacity_(cap)
        , mask_(cap - 1)
    {
    }

    [[nodiscard]] auto data_ptr() const noexcept -> const std::byte *
    {
        return reinterpret_cast<const std::byte *>(this + 1); // NOLINT
    }

    auto data_ptr() noexcept -> std::byte *
    {
        return reinterpret_cast<std::byte *>(this + 1); // NOLINT
    }

    std::size_t head_{ 0 };
    std::size_t tail_{ 0 };
    std::size_t capacity_;
    std::size_t mask_;
};

using ring_buffer_ptr = std::unique_ptr<ring_buffer, ring_buffer_deleter>;

} // namespace linyaps_box::utils
