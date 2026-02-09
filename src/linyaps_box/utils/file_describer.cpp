// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/ioctl.h"
#include "linyaps_box/utils/log.h"

#include <cstring>
#include <utility>

#include <fcntl.h>
#include <sys/uio.h>

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

linyaps_box::utils::file_descriptor::file_descriptor(int fd, bool auto_close)
    : auto_close_(auto_close)
    , fd_(fd)
{
    if (fd < 0) {
        throw file_descriptor_invalid_exception("invalid file descriptor");
    }

    fd_ = fd;

    auto flag = fcntl(*this, F_GETFL);
    if ((flag & O_NONBLOCK) != 0) {
        nonblock_ = true;
    }
}

linyaps_box::utils::file_descriptor::~file_descriptor()
{
    if (fd_ < 0 || !auto_close_) {
        return;
    }

    if (close(fd_) != 0) {
        LINYAPS_BOX_ERR() << "close " << fd_ << " failed:" << ::strerror(errno);
    }
}

linyaps_box::utils::file_descriptor::file_descriptor(file_descriptor &&other) noexcept
    : nonblock_(other.nonblock_)
    , auto_close_(other.auto_close_)
    , fd_(other.fd_)
{
    other.fd_ = -1;
}

auto linyaps_box::utils::file_descriptor::operator=(file_descriptor &&other) noexcept
        -> linyaps_box::utils::file_descriptor &
{
    if (this == &other) {
        return *this;
    }

    std::swap(this->fd_, other.fd_);
    std::swap(this->auto_close_, other.auto_close_);
    std::swap(this->nonblock_, other.nonblock_);
    return *this;
}

auto linyaps_box::utils::file_descriptor::release() -> void
{
    int tmp = -1;
    std::swap(tmp, fd_);

    if (tmp >= 0 && ::close(tmp) < 0) {
        auto msg{ "failed to close file descriptor " + std::to_string(tmp) + ": "
                  + ::strerror(errno) };
        throw file_descriptor_invalid_exception(msg);
    }
}

auto linyaps_box::utils::file_descriptor::get() const & noexcept -> int
{
    return fd_;
}

auto linyaps_box::utils::file_descriptor::get() && noexcept -> int
{
    return std::exchange(fd_, -1);
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
        throw std::system_error(errno, std::system_category(), "dup");
    }

    file_descriptor new_fd{ ret };

    // dup will lost the close-on-exec flag
    // and we don't want to use FD_DUPFD_CLOEXEC due to it require a specific fd number
    auto flag = fcntl(*this, F_GETFD);
    fcntl(new_fd, F_SETFD, flag);

    return new_fd;
}

auto linyaps_box::utils::file_descriptor::duplicate_to(int target, int flags) const -> void
{
    if (fd_ == -1) {
        throw file_descriptor_closed_exception();
    }

    if (fd_ == AT_FDCWD) {
        throw file_descriptor_invalid_exception("cannot duplicate AT_FDCWD");
    }

    auto ret = dup3(fd_, target, flags);
    if (ret < 0) {
        throw std::system_error(errno, std::system_category(), "dup3");
    }
}

auto linyaps_box::utils::file_descriptor::operator<<(const std::byte &byte)
        -> linyaps_box::utils::file_descriptor &
{
    while (true) {
        auto status = write(byte);
        switch (status) {
        case IOStatus::Success:
            return *this;
        case IOStatus::TryAgain:
            continue;
        case IOStatus::Eof:
            return *this;
        case IOStatus::Closed:
            throw file_descriptor_closed_exception();
        }
    }
    return *this;
}

auto linyaps_box::utils::file_descriptor::operator>>(std::byte &byte)
        -> linyaps_box::utils::file_descriptor &
{
    while (true) {
        auto status = read(byte);
        switch (status) {
        case IOStatus::Success:
            return *this;
        case IOStatus::TryAgain:
            continue;
        case IOStatus::Eof:
            return *this;
        case IOStatus::Closed:
            throw file_descriptor_closed_exception();
        }
    }
    return *this;
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

auto linyaps_box::utils::file_descriptor::set_nonblock(bool nonblock) -> void
{
    LINYAPS_BOX_DEBUG() << "set fd " << fd_ << " to nonblock: " << std::boolalpha << nonblock;

    auto flags = fcntl(*this, F_GETFL, 0);
    fcntl(*this,
          F_SETFL,
          nonblock ? (flags | O_NONBLOCK) : (flags & ~static_cast<unsigned>(O_NONBLOCK)));
    nonblock_ = nonblock;
}

auto linyaps_box::utils::file_descriptor::type() const -> std::filesystem::file_type
{
    auto stat = linyaps_box::utils::fstat(*this);
    return linyaps_box::utils::to_fs_file_type(stat.st_mode);
}

auto linyaps_box::utils::file_descriptor::read_span(span<std::byte> ws,
                                                    std::size_t &bytes_read) const -> IOStatus
{
    bytes_read = 0;
    if (ws.empty()) {
        return IOStatus::Success;
    }

    while (bytes_read < ws.size()) {
        const auto ret = ::read(fd_, ws.data() + bytes_read, ws.size() - bytes_read);

        if (ret > 0) {
            bytes_read += static_cast<size_t>(ret);
            continue;
        }

        if (ret == 0) {
            return bytes_read > 0 ? IOStatus::Success : IOStatus::Eof;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (nonblock_) {
                return bytes_read > 0 ? IOStatus::Success : IOStatus::TryAgain;
            }

            continue;
        }

        if (errno == EIO || errno == ECONNRESET) {
            return bytes_read > 0 ? IOStatus::Success : IOStatus::Closed;
        }

        throw std::system_error(errno, std::system_category(), "failed to read from fd");
    }

    return IOStatus::Success;
}

auto linyaps_box::utils::file_descriptor::write_span(span<const std::byte> rs,
                                                     std::size_t &bytes_written) const -> IOStatus
{
    bytes_written = 0;
    if (rs.empty()) {
        return IOStatus::Success;
    }

    while (bytes_written < rs.size()) {
        const auto ret = ::write(fd_, rs.data() + bytes_written, rs.size() - bytes_written);

        if (ret > 0) {
            bytes_written += static_cast<size_t>(ret);
            continue;
        }

        if (ret == 0) {
            return bytes_written > 0 ? IOStatus::Success : IOStatus::TryAgain;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (nonblock_) {
                return bytes_written > 0 ? IOStatus::Success : IOStatus::TryAgain;
            }

            // maybe we can retry?
            continue;
        }

        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EIO) {
            return bytes_written > 0 ? IOStatus::Success : IOStatus::Closed;
        }

        throw std::system_error(errno, std::system_category(), "failed to write to fd");
    }

    return IOStatus::Success;
}

auto linyaps_box::utils::file_descriptor::read_vecs(span<struct iovec> ws,
                                                    std::size_t &bytes_read) const -> IOStatus
{
    bytes_read = 0;
    if (ws.empty()) {
        return IOStatus::Success;
    }

    while (true) {
        const auto ret = ::readv(fd_, ws.data(), static_cast<int>(ws.size()));

        if (ret > 0) {
            bytes_read = static_cast<std::size_t>(ret);
            return IOStatus::Success;
        }

        if (ret == 0) {
            return IOStatus::Eof;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (nonblock_) {
                return IOStatus::TryAgain;
            }

            continue;
        }

        if (errno == EIO || errno == ECONNRESET) {
            return IOStatus::Closed;
        }

        throw std::system_error(errno, std::system_category(), "failed to readv from fd");
    }
}

auto linyaps_box::utils::file_descriptor::write_vecs(span<const struct iovec> rs,
                                                     std::size_t &bytes_written) const -> IOStatus
{
    bytes_written = 0;
    if (rs.empty()) {
        return IOStatus::Success;
    }

    while (true) {
        const auto ret = ::writev(fd_, rs.data(), static_cast<int>(rs.size()));

        if (ret >= 0) {
            bytes_written = static_cast<std::size_t>(ret);
            return IOStatus::Success;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (nonblock_) {
                return IOStatus::TryAgain;
            }
            continue;
        }

        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EIO) {
            return IOStatus::Closed;
        }

        throw std::system_error(errno, std::system_category(), "failed to writev to fd");
    }
}
