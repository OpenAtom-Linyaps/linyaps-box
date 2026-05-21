// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/utils.h"

#include <algorithm>
#include <array>
#include <climits> // IWYU pragma: keep
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
    : fd_(fd)
    , auto_close_(auto_close)
{
    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid file descriptor");
    }

    auto flag = fcntl(*this, F_GETFL);
    if ((flag & O_NONBLOCK) != 0) {
        nonblock_ = true;
    }
}

linyaps_box::utils::file_descriptor::~file_descriptor() noexcept
{
    if (fd_ < 0 || !auto_close_) {
        return;
    }

    // if reach here, fd must be valid
    try {
        close();
    } catch (const std::system_error &e) {
        LINYAPS_BOX_ERR() << "close " << fd_ << " failed:" << e.what();
    }
}

linyaps_box::utils::file_descriptor::file_descriptor(file_descriptor &&other) noexcept
    : fd_(other.fd_)
    , nonblock_(other.nonblock_)
    , auto_close_(other.auto_close_)
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

auto linyaps_box::utils::file_descriptor::release() & -> int
{
    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid fd");
    }

    int tmp{ -1 };
    std::swap(tmp, fd_);
    return tmp;
}

auto linyaps_box::utils::file_descriptor::close() & -> void
{
    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid fd");
    }

    auto ret = ::close(fd_);
    if (UNLIKELY(ret < 0)) {
        throw std::system_error(errno, std::system_category(), "close");
    }

    fd_ = -1;
}

auto linyaps_box::utils::file_descriptor::get() const & noexcept -> int
{
    return fd_;
}

auto linyaps_box::utils::file_descriptor::get() && -> int
{
    return std::exchange(fd_, -1);
}

auto linyaps_box::utils::file_descriptor::duplicate() const -> linyaps_box::utils::file_descriptor
{
    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid fd");
    }

    if (UNLIKELY(fd_ == AT_FDCWD)) {
        throw file_descriptor_invalid_exception("cannot duplicate AT_FDCWD");
    }

    auto ret = dup(fd_);
    if (UNLIKELY(ret < 0)) {
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
    if (UNLIKELY(fd_ == AT_FDCWD)) {
        throw file_descriptor_invalid_exception("cannot duplicate AT_FDCWD");
    }

    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid fd");
    }

    if (UNLIKELY(target < 0)) {
        throw file_descriptor_invalid_exception("target fd must be non-negative");
    }

    auto ret = dup3(fd_, target, flags);
    if (UNLIKELY(ret < 0)) {
        throw std::system_error(errno, std::system_category(), "dup3");
    }
}

auto linyaps_box::utils::file_descriptor::operator<<(const std::byte &byte)
  -> linyaps_box::utils::file_descriptor &
{
    if (UNLIKELY(nonblock_)) {
        throw std::logic_error("cannot write to non-blocking fd using operator<<");
    }

    auto [status, bytes] = write(byte);
    switch (status) {
    case IOStatus::Success:
    case IOStatus::Eof:
        [[fallthrough]];
    case IOStatus::Timeout:
        return *this;
    case IOStatus::Closed:
        throw file_descriptor_closed_exception();
    default:
        throw std::logic_error("invalid IOStatus");
    }
}

auto linyaps_box::utils::file_descriptor::operator>>(std::byte &byte)
  -> linyaps_box::utils::file_descriptor &
{
    if (UNLIKELY(nonblock_)) {
        throw std::logic_error("cannot read from non-blocking fd using operator>>");
    }

    auto [status, bytes] = read(byte);
    switch (status) {
    case IOStatus::Success:
    case IOStatus::Eof:
        [[fallthrough]];
    case IOStatus::Timeout:
        return *this;
    case IOStatus::Closed:
        throw file_descriptor_closed_exception();
    default:
        throw std::logic_error("invalid IOStatus");
    }
}

auto linyaps_box::utils::file_descriptor::proc_path() const -> std::filesystem::path
{
    if (UNLIKELY(fd_ < 0)) {
        throw file_descriptor_invalid_exception("invalid fd");
    }

    if (fd_ == AT_FDCWD) {
        return std::filesystem::current_path();
    }

    return std::filesystem::current_path().root_path() / "proc" / "self" / "fd"
      / std::to_string(fd_);
}

auto linyaps_box::utils::file_descriptor::current_path() const -> std::filesystem::path
{
    std::error_code ec;
    auto p_path = proc_path();
    auto path = std::filesystem::read_symlink(p_path, ec);
    if (UNLIKELY(!!ec)) {
        LINYAPS_BOX_ERR() << "failed to read symlink " << p_path.c_str() << ": " << ec.message();
    }

    return path;
}

auto linyaps_box::utils::file_descriptor::cwd() -> file_descriptor
{
    return file_descriptor{ AT_FDCWD };
}

auto linyaps_box::utils::file_descriptor::set_nonblock(bool nonblock) & -> void
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

auto linyaps_box::utils::file_descriptor::read_span(span<std::byte> ws) const -> IOResult
{
    if (ws.empty()) {
        return { IOStatus::Success, 0 };
    }

    while (true) {
        const auto ret = ::read(fd_, ws.data(), ws.size());

        if (ret > 0) {
            return { IOStatus::Success, static_cast<std::size_t>(ret) };
        }

        if (ret == 0) {
            return { IOStatus::Eof, 0 };
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return { nonblock_ ? IOStatus::TryAgain : IOStatus::Timeout, 0 };
        }

        if (errno == EIO || errno == ECONNRESET) {
            return { IOStatus::Closed, 0 };
        }

        if (errno == EBADF) {
            throw std::system_error(errno, std::system_category(), "invalid fd");
        }

        throw std::system_error(errno, std::system_category(), "failed to read from fd");
    }
}

auto linyaps_box::utils::file_descriptor::write_span(span<const std::byte> rs) const -> IOResult
{
    if (rs.empty()) {
        return { IOStatus::Success, 0 };
    }

    std::size_t bytes_written{ 0 };
    while (bytes_written < rs.size()) {
        const auto ret = ::write(fd_, rs.data() + bytes_written, rs.size() - bytes_written);

        if (ret > 0) {
            bytes_written += static_cast<size_t>(ret);
            continue;
        }

        if (ret == 0) {
            return { bytes_written > 0 ? IOStatus::Success : IOStatus::TryAgain, bytes_written };
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return { nonblock_ ? IOStatus::TryAgain : IOStatus::Timeout, bytes_written };
        }

        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EIO) {
            return { IOStatus::Closed, bytes_written };
        }

        if (errno == EBADF) {
            throw std::system_error(errno, std::system_category(), "invalid fd");
        }

        throw std::system_error(errno, std::system_category(), "failed to write to fd");
    }

    return { IOStatus::Success, bytes_written };
}

auto linyaps_box::utils::file_descriptor::read_vecs(span<struct iovec> ws) const -> IOResult
{
    if (ws.empty()) {
        return { IOStatus::Success, 0 };
    }

    const int iov_cnt = static_cast<int>(std::min(ws.size(), static_cast<std::size_t>(IOV_MAX)));
    while (true) {
        // Opportunistic read for non-blocking I/O: a single readv call is intentional.
        // Short reads are expected; data is dispatched immediately to the state machine without
        // looping.
        const auto ret = ::readv(fd_, ws.data(), iov_cnt);

        if (ret > 0) {
            return { IOStatus::Success, static_cast<std::size_t>(ret) };
        }

        if (ret == 0) {
            return { IOStatus::Eof, 0 };
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return { nonblock_ ? IOStatus::TryAgain : IOStatus::Timeout, 0 };
        }

        if (errno == EIO || errno == ECONNRESET) {
            return { IOStatus::Closed, 0 };
        }

        if (errno == EBADF) {
            throw std::system_error(errno, std::system_category(), "invalid fd");
        }

        throw std::system_error(errno, std::system_category(), "failed to readv from fd");
    }
}

auto linyaps_box::utils::file_descriptor::write_vecs(span<const struct iovec> rs) const -> IOResult
{
    if (rs.empty()) {
        return { IOStatus::Success, 0 };
    }

    std::size_t total_bytes_written{ 0 };
    std::size_t next_chunk_idx{ 0 };

    constexpr auto kMaxStackIov = 64;
    std::array<struct iovec, kMaxStackIov> iov_stack_buf;    // NOLINT
    std::unique_ptr<struct iovec[]> iov_heap_buf{ nullptr }; // NOLINT
    bool using_mutable_buf{ false };

    const struct iovec *current_iov_ptr{ nullptr };
    std::size_t active_chunks{ 0 };

    while (next_chunk_idx < rs.size() || active_chunks > 0) {
        if (active_chunks == 0) {
            active_chunks = std::min(rs.size() - next_chunk_idx, static_cast<std::size_t>(IOV_MAX));
            current_iov_ptr = &rs[next_chunk_idx]; // NOLINT
            next_chunk_idx += active_chunks;
            using_mutable_buf = false;
        }

        const auto ret = ::writev(fd_, current_iov_ptr, static_cast<int>(active_chunks));

        if (ret > 0) {
            auto bytes_to_consume = static_cast<std::size_t>(ret);
            total_bytes_written += bytes_to_consume;

            while (bytes_to_consume > 0 && active_chunks > 0) {
                if (bytes_to_consume >= current_iov_ptr->iov_len) {
                    bytes_to_consume -= current_iov_ptr->iov_len;
                    current_iov_ptr = std::next(current_iov_ptr);
                    --active_chunks;
                } else {
                    if (!using_mutable_buf) {
                        if (active_chunks > kMaxStackIov) {
                            iov_heap_buf = std::make_unique<struct iovec[]>(active_chunks);
                            std::copy_n(current_iov_ptr, active_chunks, iov_heap_buf.get());
                            current_iov_ptr = iov_heap_buf.get();
                        } else {
                            std::copy_n(current_iov_ptr, active_chunks, iov_stack_buf.data());
                            current_iov_ptr = iov_stack_buf.data();
                        }
                        using_mutable_buf = true;
                    }

                    auto *mutable_iov = const_cast<struct iovec *>(current_iov_ptr); // NOLINT
                    mutable_iov->iov_base =
                      static_cast<char *>(mutable_iov->iov_base) + bytes_to_consume;
                    mutable_iov->iov_len -= bytes_to_consume;
                    bytes_to_consume = 0;
                }
            }
            continue;
        }

        if (ret == 0) {
            return { total_bytes_written > 0 ? IOStatus::Success : IOStatus::TryAgain,
                     total_bytes_written };
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return { nonblock_ ? IOStatus::TryAgain : IOStatus::Timeout, total_bytes_written };
        }

        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EIO) {
            return { IOStatus::Closed, total_bytes_written };
        }

        if (errno == EBADF) {
            throw std::system_error(errno, std::system_category(), "invalid fd");
        }

        throw std::system_error(errno, std::system_category(), "failed to writev to fd");
    }

    return { IOStatus::Success, total_bytes_written };
}
