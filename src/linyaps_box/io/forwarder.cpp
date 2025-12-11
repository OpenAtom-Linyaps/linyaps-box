// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/forwarder.h"

#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/utils.h"

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
                        << ", pollable: " << src_.pollable;
}

auto Forwarder::set_dst(const utils::file_descriptor &dst) -> void
{
    dst_.fd = &dst;
    const auto ev = EPOLLOUT | EPOLLET;

    dst_.pollable = poller.get().add(*dst_.fd, ev);
    dst_.last_events = dst_.pollable ? ev : 0;

    LINYAPS_BOX_DEBUG() << "Forwarder: Destination fd: " << dst_.fd->get()
                        << ", pollable: " << dst_.pollable;
}

auto Forwarder::handle_forwarding() -> Status
{
    if (UNLIKELY((src_.fd == nullptr) || (dst_.fd == nullptr))) {
        throw std::runtime_error("src or dst is not set");
    }

    bool moved{ false };
    while (!rb->empty()) {
        auto vecs = rb->get_read_vecs();
        const auto iov_cnt = (vecs[1].iov_len > 0) ? 2U : 1U;

        std::size_t bytes_written{ 0 };
        auto status = dst_.fd->write_vecs({ vecs.data(), iov_cnt }, bytes_written);

        if (bytes_written > 0) {
            rb->advance_head(bytes_written);
            moved = true;
        }

        if (status == utils::file_descriptor::IOStatus::Success) {
            if (bytes_written == 0) {
                break;
            }

            const std::size_t requested = vecs[0].iov_len + vecs[1].iov_len;
            if (bytes_written < requested) {
                break;
            }

            continue;
        }

        if (status == utils::file_descriptor::IOStatus::Closed) {
            return Status::Finished;
        }

        break;
    }

    if (!src_eof) {
        while (!rb->full()) {
            auto vecs = rb->get_write_vecs();
            const auto iov_cnt = (vecs[1].iov_len > 0) ? 2U : 1U;

            std::size_t bytes_read{ 0 };
            auto status = src_.fd->read_vecs({ vecs.data(), iov_cnt }, bytes_read);

            if (bytes_read > 0) {
                rb->advance_tail(bytes_read);
                moved = true;
            }

            if (status == utils::file_descriptor::IOStatus::Eof
                || status == utils::file_descriptor::IOStatus::Closed) {
                src_eof = true;
                break;
            }

            if (status == utils::file_descriptor::IOStatus::Success) {
                if (bytes_read == 0) {
                    break;
                }

                const std::size_t requested = vecs[0].iov_len + vecs[1].iov_len;
                if (bytes_read < requested) {
                    break;
                }

                continue;
            }

            if (status == utils::file_descriptor::IOStatus::TryAgain) {
                break;
            }
        }
    }

    update_interests();

    if (src_eof) {
        return rb->empty() ? Status::Finished : Status::SourceClosed;
    }

    return moved ? Status::Busy : Status::Idle;
}

auto Forwarder::update_interests() -> void
{
    auto update = [this](FdContext &ctx, uint32_t next) {
        if (!ctx.pollable) {
            return;
        }

        if (ctx.last_events != next) {
            this->poller.get().modify(*ctx.fd, next);
            ctx.last_events = next;
        }
    };

    update(src_, (src_eof || rb->full()) ? 0 : (EPOLLIN | EPOLLET));
    update(dst_, rb->empty() ? 0 : (EPOLLOUT | EPOLLET));
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
