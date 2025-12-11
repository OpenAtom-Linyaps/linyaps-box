// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/io/epoll.h"

#include "linyaps_box/utils/epoll.h"

namespace linyaps_box::io {

Epoll::Epoll(linyaps_box::utils::file_descriptor &&fd)
    : epoll_fd{ std::move(fd) }
{
    events_buffer.resize(10);
}

Epoll::Epoll(bool close_on_exec)
    : epoll_fd{ linyaps_box::utils::epoll_create1(close_on_exec ? EPOLL_CLOEXEC : 0) }
{
}

auto Epoll::add(const utils::file_descriptor &fd, uint32_t events) -> bool
{
    struct epoll_event event{};
    event.events = events;
    event.data.fd = fd.get();

    try {
        linyaps_box::utils::epoll_ctl(epoll_fd,
                                      linyaps_box::utils::epoll_operation::add,
                                      fd,
                                      &event);
    } catch (std::system_error &e) {
        if (e.code().value() == EPERM) {
            return false;
        }

        throw;
    }

    if (events_buffer.size() == events_buffer.capacity()) {
        events_buffer.reserve(events_buffer.capacity() * 2);
    }

    events_buffer.resize(events_buffer.size() + 1);
    return true;
}

void Epoll::modify(const utils::file_descriptor &fd, uint32_t events)
{
    struct epoll_event event{};
    event.events = events;
    event.data.fd = fd.get();

    linyaps_box::utils::epoll_ctl(epoll_fd,
                                  linyaps_box::utils::epoll_operation::modify,
                                  fd,
                                  &event);
}

void Epoll::remove(const utils::file_descriptor &fd)
{
    linyaps_box::utils::epoll_ctl(epoll_fd,
                                  linyaps_box::utils::epoll_operation::remove,
                                  fd,
                                  nullptr);
}

auto Epoll::wait(int timeout) -> const std::vector<struct epoll_event> &
{
    auto nevents = linyaps_box::utils::epoll_wait(epoll_fd, events_buffer, timeout);
    events_buffer.resize(nevents);
    return events_buffer;
}

} // namespace linyaps_box::io
