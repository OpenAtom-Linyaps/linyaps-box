// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/io/epoll.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/ringbuffer.h"

namespace linyaps_box::io {

class Forwarder
{
public:
    explicit Forwarder(Epoll &poller, std::size_t buffer_size = BUFSIZ);

    Forwarder(const Forwarder &) = delete;
    Forwarder &operator=(const Forwarder &) = delete;
    Forwarder(Forwarder &&) = delete;
    Forwarder &operator=(Forwarder &&) = delete;

    ~Forwarder() noexcept;

    auto set_src(const utils::file_descriptor &src) -> void;

    [[nodiscard]] auto src() const noexcept -> const utils::file_descriptor & { return *src_.fd; }

    auto set_dst(const utils::file_descriptor &dst) -> void;

    [[nodiscard]] auto dst() const noexcept -> const utils::file_descriptor & { return *dst_.fd; }

    auto mark_src_eof() noexcept -> void { src_eof = true; }

    auto mark_dst_failed() noexcept -> void { dst_failed = true; }

    [[nodiscard]] auto is_finished() const noexcept -> bool
    {
        return (src_eof && buffer_empty()) || dst_failed;
    }

    auto drive() -> bool;

    [[nodiscard]] auto buffer_empty() const noexcept -> bool { return rb ? rb->empty() : true; }

private:
    struct FdContext
    {
        const utils::file_descriptor *fd{ nullptr };
        bool pollable{ false };
    };

    [[nodiscard]] auto pull(std::size_t &bytes_quota) -> bool;
    [[nodiscard]] auto push(std::size_t &bytes_quota) -> bool;

    utils::ring_buffer::ptr rb;
    FdContext src_;
    FdContext dst_;
    std::reference_wrapper<Epoll> poller;
    bool src_eof{ false };
    bool dst_failed{ false };
};

} // namespace linyaps_box::io
