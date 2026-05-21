// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/epoll.h"

#include "linyaps_box/utils/epoll.h"

namespace linyaps_box::io {

Epoll::Epoll(utils::file_descriptor &&fd) noexcept
    : epoll_fd{ std::move(fd) }
{
}

Epoll::Epoll(bool close_on_exec)
    : Epoll(utils::epoll_create1(close_on_exec ? EPOLL_CLOEXEC : 0))
{
}

auto Epoll::add(const utils::file_descriptor &fd, uint32_t events) -> bool
{
    struct epoll_event event{ };
    event.events = events;
    event.data.fd = fd.get();

    try {
        utils::epoll_ctl(epoll_fd, utils::epoll_operation::add, fd, &event);
    } catch (const std::system_error &e) {
        if (e.code().value() == EPERM) {
            return false;
        }

        throw;
    }

    return true;
}

void Epoll::modify(const utils::file_descriptor &fd, uint32_t events)
{
    struct epoll_event event{ };
    event.events = events;
    event.data.fd = fd.get();

    utils::epoll_ctl(epoll_fd, utils::epoll_operation::modify, fd, &event);
}

void Epoll::remove(const utils::file_descriptor &fd)
{
    utils::epoll_ctl(epoll_fd, linyaps_box::utils::epoll_operation::remove, fd, nullptr);
}

auto Epoll::wait(int timeout) -> utils::span<const struct epoll_event>
{
    auto nevents = utils::epoll_wait(epoll_fd, events_buffer.data(), events_buffer.size(), timeout);
    return { events_buffer.data(), static_cast<std::size_t>(nevents) };
}

} // namespace linyaps_box::io
