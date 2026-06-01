// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/epoll.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"

#include <sys/epoll.h>

#include <array>

namespace linyaps_box::io {

constexpr std::size_t event_buffer_size = 16;

class Epoll
{
public:
    explicit Epoll(bool close_on_exec = true);
    explicit Epoll(linyaps_box::utils::file_descriptor &&fd) noexcept;

    ~Epoll() = default;
    Epoll(const Epoll &) = delete;
    Epoll &operator=(const Epoll &) = delete;
    Epoll(Epoll &&) = default;
    Epoll &operator=(Epoll &&) = default;

    // false if fd doesn't support epoll
    [[nodiscard]] auto add(const utils::file_descriptor &fd, uint32_t events) -> bool;

    auto modify(const utils::file_descriptor &fd, uint32_t events) -> void;

    auto remove(const utils::file_descriptor &fd) -> void;

    auto wait(int timeout) -> utils::span<const struct epoll_event>;

private:
    std::array<struct epoll_event, event_buffer_size> events_buffer{ };
    linyaps_box::utils::file_descriptor epoll_fd;
};

} // namespace linyaps_box::io
