// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/forwarder.h"

#include "linyaps_box/utils/log.h"

#include <algorithm>
#include <cstddef>

namespace linyaps_box::io {

static constexpr std::size_t default_bytes_quota{ static_cast<std::size_t>(16 * 1024) };

Forwarder::Forwarder(Epoll &poller, std::size_t buffer_size)
    : rb(utils::ring_buffer::create(buffer_size))
    , poller(poller)
{
}

auto Forwarder::set_src(const utils::file_descriptor &src) -> void
{
    src_.fd = &src;
    const auto ev = EPOLLIN | EPOLLET;

    src_.pollable = poller.get().add(*src_.fd, ev);

    LINYAPS_BOX_DEBUG() << "Forwarder: Source fd: " << src_.fd->get()
                        << ", pollable: " << std::boolalpha << src_.pollable;
}

auto Forwarder::set_dst(const utils::file_descriptor &dst) -> void
{
    dst_.fd = &dst;
    const uint32_t ev = EPOLLOUT | EPOLLET;

    dst_.pollable = poller.get().add(*dst_.fd, ev);

    LINYAPS_BOX_DEBUG() << "Forwarder: Destination fd: " << dst_.fd->get()
                        << ", pollable: " << std::boolalpha << dst_.pollable;
}

auto Forwarder::drive() -> bool
{
    auto io_quota{ default_bytes_quota };
    bool is_completely_blocked{ false };

    while (io_quota > 0) {
        auto quota_before = io_quota;

        const auto read_would_block = pull(io_quota);
        const auto write_would_block = push(io_quota);

        const auto cannot_read = read_would_block || rb->full();
        const auto cannot_write = write_would_block || rb->empty();

        if (cannot_read && cannot_write) {
            is_completely_blocked = true;
            break;
        }

        if (io_quota == quota_before) {
            break;
        }
    }

    return !is_completely_blocked;
}

auto Forwarder::pull(std::size_t &bytes_quota) -> bool
{
    if (bytes_quota == 0) {
        return false;
    }

    while (bytes_quota > 0 && !rb->full()) {
        auto *ptr = rb->get_write_ptr();
        const utils::span<std::byte> span(ptr, rb->free_space());

        auto [status, bytes_read] = src_.fd->read_span(span);
        if (status == utils::IOStatus::TryAgain) {
            return true;
        }

        if (status != utils::IOStatus::Success) {
            mark_src_eof();
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        rb->advance_head(bytes_read);
        bytes_quota -= std::min(bytes_quota, bytes_read);
    }

    return false;
}

auto Forwarder::push(std::size_t &bytes_quota) -> bool
{
    if (bytes_quota == 0) {
        return false;
    }

    bool partial_written{ false };
    while (!rb->empty()) {
        if (bytes_quota == 0 && !partial_written) {
            break;
        }

        auto *ptr = rb->get_read_ptr();
        const utils::span<const std::byte> span(ptr, rb->size());

        auto [status, bytes_written] = dst_.fd->write_span(span);
        if (status == utils::IOStatus::TryAgain) {
            return true;
        }

        if (status != utils::IOStatus::Success) {
            mark_dst_failed();
            rb->clear();
            return true;
        }

        if (bytes_written == 0) {
            break;
        }

        rb->advance_tail(bytes_written);
        bytes_quota -= std::min(bytes_quota, bytes_written);
        partial_written = (bytes_written < span.size());
    }

    return false;
}

Forwarder::~Forwarder() noexcept
{
    try {
        if (src_.fd != nullptr && src_.pollable) {
            this->poller.get().remove(*src_.fd);
        }
    } catch (const std::exception &e) {
        LINYAPS_BOX_ERR() << "Failed to remove source fd " << src_.fd->get()
                          << " from epoll: " << e.what();
    }

    try {
        if (dst_.fd != nullptr && dst_.pollable) {
            this->poller.get().remove(*dst_.fd);
        }
    } catch (const std::exception &e) {
        LINYAPS_BOX_ERR() << "Failed to remove destination fd " << dst_.fd->get()
                          << " from epoll: " << e.what();
    }
}

} // namespace linyaps_box::io
