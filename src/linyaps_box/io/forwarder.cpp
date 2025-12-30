// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/forwarder.h"

#include "linyaps_box/utils/log.h"

namespace linyaps_box::io {

Forwarder::Forwarder(Epoll &poller, std::size_t buffer_size)
    : poller(poller)
    , rb(utils::ring_buffer::create(buffer_size))
{
}

auto Forwarder::set_src(const utils::file_descriptor &src) -> void
{
    src_.fd = &src;
    const auto ev = EPOLLIN | EPOLLET;

    src_.pollable = poller.get().add(*src_.fd, ev);
    src_.last_events = src_.pollable ? ev : 0;

    LINYAPS_BOX_DEBUG() << "Forwarder: Source fd: " << src_.fd->get()
                        << ", pollable: " << std::boolalpha << src_.pollable;
}

auto Forwarder::set_dst(const utils::file_descriptor &dst) -> void
{
    dst_.fd = &dst;
    const auto ev = EPOLLET;

    dst_.pollable = poller.get().add(*dst_.fd, ev);
    dst_.last_events = dst_.pollable ? ev : 0;

    LINYAPS_BOX_DEBUG() << "Forwarder: Destination fd: " << dst_.fd->get()
                        << ", pollable: " << std::boolalpha << dst_.pollable;
}

auto Forwarder::pull() -> Status
{
    if (src_eof) {
        return rb->empty() ? Status::Finished : Status::Blocked;
    }

    if (rb->full()) {
        return Status::Blocked;
    }

    last_pull_again = false;
    while (!rb->full()) {
        auto vecs = rb->get_write_vecs();
        const auto iov_cnt = (vecs[1].iov_len > 0) ? 2U : 1U;

        std::size_t bytes_read{ 0 };
        auto status = src_.fd->read_vecs({ vecs.data(), iov_cnt }, bytes_read);

        if (bytes_read > 0) {
            rb->advance_tail(bytes_read);
        }

        if (status == utils::file_descriptor::IOStatus::TryAgain) {
            last_pull_again = true;
            break;
        }

        if (status != utils::file_descriptor::IOStatus::Success) {
            src_eof = true;
            update_event(src_, false);
            break;
        }

        if (!src_.pollable) {
            break;
        }
    }

    if (!src_eof && !rb->full() && !last_pull_again) {
        return Status::Continue;
    }

    return Status::Blocked;
}

auto Forwarder::push() -> Status
{
    if (rb->empty()) {
        return src_eof ? Status::Finished : Status::Blocked;
    }

    last_push_again = false;
    while (!rb->empty()) {
        auto vecs = rb->get_read_vecs();
        const auto iov_cnt = (vecs[1].iov_len > 0) ? 2U : 1U;

        std::size_t bytes_write{ 0 };
        auto status = dst_.fd->write_vecs({ vecs.data(), iov_cnt }, bytes_write);

        if (bytes_write > 0) {
            rb->advance_head(bytes_write);
        }

        if (status == utils::file_descriptor::IOStatus::TryAgain) {
            last_push_again = true;
            update_event(dst_, true);
            break;
        }

        if (status != utils::file_descriptor::IOStatus::Success) {
            return Status::Finished;
        }

        if (!dst_.pollable) {
            break;
        }
    }

    if (src_eof && rb->empty()) {
        return Status::Finished;
    }

    if (!rb->empty() && !last_push_again) {
        return Status::Continue;
    }

    if (rb->empty()) {
        update_event(dst_, false);
    }

    return Status::Blocked;
}

auto Forwarder::update_event(FdContext &ctx, bool on) -> void
{
    if (!ctx.pollable) {
        return;
    }

    uint32_t new_events = EPOLLET;

    if (ctx.fd == src_.fd) {
        if (on) {
            new_events |= EPOLLIN;
        }
    } else if (ctx.fd == dst_.fd) {
        if (on) {
            new_events |= EPOLLOUT;
        }
    }

    if (ctx.last_events != new_events) {
        poller.get().modify(*ctx.fd, new_events);
        ctx.last_events = new_events;
    }
}

Forwarder::~Forwarder() noexcept
{
    try {
        if (src_.fd != nullptr && src_.pollable) {
            this->poller.get().remove(*src_.fd);
        }
    } catch (std::system_error &e) {
        LINYAPS_BOX_ERR() << "Failed to remove source fd " << src_.fd->get()
                          << " from epoll: " << e.what();
    }

    try {
        if (dst_.fd != nullptr && dst_.pollable) {
            this->poller.get().remove(*dst_.fd);
        }
    } catch (std::system_error &e) {
        LINYAPS_BOX_ERR() << "Failed to remove destination fd " << dst_.fd->get()
                          << " from epoll: " << e.what();
    }
}

} // namespace linyaps_box::io
