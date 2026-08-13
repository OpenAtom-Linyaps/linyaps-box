// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/file_describer.h"
#include "linyaps_box/utils/span.h"

#include <type_traits>

#include <sys/uio.h>

namespace linyaps_box::os {

class io_slice
{
public:
    constexpr io_slice() noexcept = default;
    constexpr io_slice(const io_slice &) = default;
    constexpr io_slice(io_slice &&) noexcept = default;
    constexpr io_slice &operator=(const io_slice &) = default;
    constexpr io_slice &operator=(io_slice &&) noexcept = default;
    ~io_slice() noexcept = default;

    template <typename T>
    constexpr explicit io_slice(utils::span<T> s) noexcept
    {
        auto bytes = utils::as_bytes(s);
        iov_.iov_base = const_cast<void *>(static_cast<const void *>(bytes.data()));
        iov_.iov_len = bytes.size();
    }

    [[nodiscard]] constexpr auto ptr() const noexcept -> const void * { return iov_.iov_base; }

    [[nodiscard]] constexpr auto len() const noexcept { return iov_.iov_len; }

    [[nodiscard]] constexpr auto iov() noexcept -> struct iovec * { return &iov_; }

    [[nodiscard]] constexpr auto iov() const noexcept -> const struct iovec * { return &iov_; }

private:
    struct iovec iov_{ };
};

static_assert(sizeof(io_slice) == sizeof(struct iovec), "Size mismatch with struct iovec");
static_assert(alignof(io_slice) == alignof(struct iovec), "Alignment mismatch with struct iovec");
static_assert(std::is_standard_layout_v<io_slice>, "io_slice must be standard layout");

class mutable_io_slice
{
public:
    constexpr mutable_io_slice() noexcept = default;
    constexpr mutable_io_slice(const mutable_io_slice &) = default;
    constexpr mutable_io_slice(mutable_io_slice &&) noexcept = default;
    constexpr mutable_io_slice &operator=(const mutable_io_slice &) = default;
    constexpr mutable_io_slice &operator=(mutable_io_slice &&) noexcept = default;
    ~mutable_io_slice() noexcept = default;

    template <typename T, typename = std::enable_if_t<!std::is_const_v<T>>>
    constexpr explicit mutable_io_slice(utils::span<T> s) noexcept
    {
        auto bytes = utils::as_writable_bytes(s);
        iov_.iov_base = bytes.data();
        iov_.iov_len = bytes.size();
    }

    constexpr operator io_slice() const noexcept
    {
        return io_slice{ utils::span<const std::byte>{
          static_cast<const std::byte *>(iov_.iov_base),
          iov_.iov_len } };
    }

    [[nodiscard]] constexpr auto ptr() const noexcept { return iov_.iov_base; }

    [[nodiscard]] constexpr auto len() const noexcept { return iov_.iov_len; }

    [[nodiscard]] constexpr auto iov() noexcept -> struct iovec * { return &iov_; }

    [[nodiscard]] constexpr auto iov() const noexcept -> const struct iovec * { return &iov_; }

private:
    struct iovec iov_{ };
};

static_assert(sizeof(mutable_io_slice) == sizeof(struct iovec), "Size mismatch with struct iovec");
static_assert(alignof(mutable_io_slice) == alignof(struct iovec),
              "Alignment mismatch with struct iovec");
static_assert(std::is_standard_layout_v<mutable_io_slice>,
              "mutable_io_slice must be standard layout");

auto read(utils::file_descriptor_ref fd, utils::span<std::byte> buf) noexcept
  -> Result<std::size_t>;

} // namespace linyaps_box::os
