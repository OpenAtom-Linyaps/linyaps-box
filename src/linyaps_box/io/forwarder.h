// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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
    enum class Status : uint8_t { Busy, Idle, SourceClosed, Finished };

    explicit Forwarder(Epoll &poller, std::size_t buffer_size = BUFSIZ);

    Forwarder(const Forwarder &) = delete;
    Forwarder &operator=(const Forwarder &) = delete;
    Forwarder(Forwarder &&) = delete;
    Forwarder &operator=(Forwarder &&) = delete;

    ~Forwarder() noexcept;

    auto set_src(const utils::file_descriptor &src) -> void;

    auto set_dst(const utils::file_descriptor &dst) -> void;

    [[nodiscard]] auto is_src_pollable() const noexcept -> bool { return src_.pollable; };

    [[nodiscard]] auto is_dst_pollable() const noexcept -> bool { return dst_.pollable; }

    [[nodiscard]] auto handle_forwarding() -> Status;

private:
    struct FdContext
    {
        const utils::file_descriptor *fd{ nullptr };
        uint32_t last_events{ std::numeric_limits<uint32_t>::max() };
        bool pollable{ false };
    };

    void update_interests();

    bool src_eof{ false };
    std::reference_wrapper<Epoll> poller;
    FdContext src_;
    FdContext dst_;
    utils::ring_buffer_ptr rb;
};

} // namespace linyaps_box::io
