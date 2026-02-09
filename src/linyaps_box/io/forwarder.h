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
    enum class Status : uint8_t { Continue, Blocked, Finished };

    explicit Forwarder(Epoll &poller, std::size_t buffer_size = BUFSIZ);

    Forwarder(const Forwarder &) = delete;
    Forwarder &operator=(const Forwarder &) = delete;
    Forwarder(Forwarder &&) = delete;
    Forwarder &operator=(Forwarder &&) = delete;

    ~Forwarder() noexcept;

    auto set_src(const utils::file_descriptor &src) -> void;

    [[nodiscard]] auto src() const -> const utils::file_descriptor & { return *src_.fd; }

    auto set_dst(const utils::file_descriptor &dst) -> void;

    [[nodiscard]] auto dst() const -> const utils::file_descriptor & { return *dst_.fd; }

    auto pull() -> Status;

    [[nodiscard]] auto push() -> Status;

private:
    struct FdContext
    {
        const utils::file_descriptor *fd{ nullptr };
        uint32_t last_events{ 0 };
        bool pollable{ false };
    };

    auto update_event(FdContext &ctx, bool on) -> void;

    bool src_eof{ false };
    bool last_pull_again{ false };
    bool last_push_again{ false };

    std::reference_wrapper<Epoll> poller;
    FdContext src_;
    FdContext dst_;
    utils::ring_buffer_ptr rb;
};

} // namespace linyaps_box::io
