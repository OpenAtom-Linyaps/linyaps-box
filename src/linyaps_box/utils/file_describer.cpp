// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include "linyaps_box/utils/fstat.h"
#include "linyaps_box/utils/log.h"

#include <cstring>

#include <fcntl.h>

linyaps_box::utils::file_descriptor_closed_exception::file_descriptor_closed_exception()
    : std::runtime_error("file descriptor is closed")
{
}

linyaps_box::utils::file_descriptor::file_descriptor(int fd)
    : fd(fd)
{
}

linyaps_box::utils::file_descriptor::~file_descriptor()
{
    if (fd == -1) {
        return;
    }

    if (close(fd) != 0) {
        LINYAPS_BOX_ERR() << "close " << fd << " failed:" << ::strerror(errno);
    }
}

linyaps_box::utils::file_descriptor::file_descriptor(file_descriptor &&other) noexcept
{
    *this = std::move(other);
}

int linyaps_box::utils::file_descriptor::release() && noexcept
{
    int ret = -1;
    std::swap(ret, fd);

    return ret;
}

int linyaps_box::utils::file_descriptor::get() const noexcept
{
    return fd;
}

linyaps_box::utils::file_descriptor linyaps_box::utils::file_descriptor::duplicate() const
{
    if (fd == -1) {
        throw file_descriptor_closed_exception();
    }

    auto ret = dup(fd);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    // dup will lost the close-on-exec flag
    // and we don't want to use FD_DUPFD_CLOEXEC due to it require a specific fd number
    auto flag = fcntl(fd, F_GETFD);
    if (flag < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    if (fcntl(ret, F_SETFD, flag) < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    return file_descriptor{ ret };
}

linyaps_box::utils::file_descriptor &
linyaps_box::utils::file_descriptor::operator<<(const std::byte &byte)
{
    while (true) {
        auto ret = write(fd, &byte, 1);
        if (ret == 1) {
            return *this;
        }
        if (ret == 0 || errno == EINTR || errno == EAGAIN) {
            continue;
        }
        throw std::system_error(errno, std::generic_category(), "write");
    }
}

linyaps_box::utils::file_descriptor &
linyaps_box::utils::file_descriptor::operator=(file_descriptor &&other) noexcept
{
    std::swap(this->fd, other.fd);
    return *this;
}

linyaps_box::utils::file_descriptor &
linyaps_box::utils::file_descriptor::operator>>(std::byte &byte)
{
    while (true) {
        auto ret = read(fd, &byte, 1);
        if (ret == 1) {
            return *this;
        }
        if (ret == 0) {
            throw file_descriptor_closed_exception();
        }
        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }
        throw std::system_error(errno, std::generic_category(), "read");
    }
}

std::filesystem::path linyaps_box::utils::file_descriptor::proc_path() const
{
    return std::filesystem::current_path().root_path() / "proc" / "self" / "fd"
            / std::to_string(fd);
}

std::filesystem::path linyaps_box::utils::file_descriptor::current_path() const noexcept
{
    std::error_code ec;
    auto p_path = proc_path();
    auto path = std::filesystem::read_symlink(p_path, ec);
    if (ec) {
        LINYAPS_BOX_ERR() << "failed to read symlink " << p_path.c_str() << ": " << ec.message();
    }

    return path;
}
