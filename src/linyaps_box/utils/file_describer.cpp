// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include "linyaps_box/utils/log.h"

#include <cstring>

#include <fcntl.h>

linyaps_box::utils::file_descriptor_closed_exception::file_descriptor_closed_exception()
    : std::runtime_error("file descriptor is closed")
{
}

linyaps_box::utils::file_descriptor_closed_exception::~file_descriptor_closed_exception() noexcept =
        default;

linyaps_box::utils::file_descriptor_invalid_exception::file_descriptor_invalid_exception(
        const std::string &message)
    : std::runtime_error(message)
{
}

linyaps_box::utils::file_descriptor_invalid_exception::
        ~file_descriptor_invalid_exception() noexcept = default;

linyaps_box::utils::file_descriptor::file_descriptor(int fd)
    : fd_(fd)
{
}

linyaps_box::utils::file_descriptor::~file_descriptor()
{
    if (fd_ < 0) {
        return;
    }

    if (close(fd_) != 0) {
        LINYAPS_BOX_ERR() << "close " << fd_ << " failed:" << ::strerror(errno);
    }
}

linyaps_box::utils::file_descriptor::file_descriptor(file_descriptor &&other) noexcept
{
    *this = std::move(other);
}

auto linyaps_box::utils::file_descriptor::release() && noexcept -> int
{
    int ret = -1;
    std::swap(ret, fd_);

    return ret;
}

auto linyaps_box::utils::file_descriptor::get() const noexcept -> int
{
    return fd_;
}

auto linyaps_box::utils::file_descriptor::duplicate() const -> linyaps_box::utils::file_descriptor
{
    if (fd_ == -1) {
        throw file_descriptor_closed_exception();
    }

    if (fd_ == AT_FDCWD) {
        throw file_descriptor_invalid_exception("cannot duplicate AT_FDCWD");
    }

    auto ret = dup(fd_);
    if (ret < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    // dup will lost the close-on-exec flag
    // and we don't want to use FD_DUPFD_CLOEXEC due to it require a specific fd number
    auto flag = fcntl(fd_, F_GETFD);
    if (flag < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    if (fcntl(ret, F_SETFD, flag) < 0) {
        throw std::system_error(errno, std::generic_category(), "fcntl");
    }

    return file_descriptor{ ret };
}

auto linyaps_box::utils::file_descriptor::operator<<(const std::byte &byte)
        -> linyaps_box::utils::file_descriptor &
{
    while (true) {
        auto ret = write(fd_, &byte, 1);
        if (ret == 1) {
            return *this;
        }
        if (ret == 0 || errno == EINTR || errno == EAGAIN) {
            continue;
        }
        throw std::system_error(errno, std::generic_category(), "write");
    }
}

auto linyaps_box::utils::file_descriptor::operator=(file_descriptor &&other) noexcept
        -> linyaps_box::utils::file_descriptor &
{
    std::swap(this->fd_, other.fd_);
    return *this;
}

auto linyaps_box::utils::file_descriptor::operator>>(std::byte &byte)
        -> linyaps_box::utils::file_descriptor &
{
    while (true) {
        auto ret = read(fd_, &byte, 1);
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

auto linyaps_box::utils::file_descriptor::proc_path() const -> std::filesystem::path
{
    return std::filesystem::current_path().root_path() / "proc" / "self" / "fd"
            / std::to_string(fd_);
}

auto linyaps_box::utils::file_descriptor::current_path() const noexcept -> std::filesystem::path
{
    std::error_code ec;
    auto p_path = proc_path();
    auto path = std::filesystem::read_symlink(p_path, ec);
    if (ec) {
        LINYAPS_BOX_ERR() << "failed to read symlink " << p_path.c_str() << ": " << ec.message();
    }

    return path;
}

auto linyaps_box::utils::file_descriptor::cwd() -> file_descriptor
{
    return file_descriptor{ AT_FDCWD };
}
