// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/span.h"

#include <filesystem>

#include <sys/uio.h>
#include <unistd.h>

namespace linyaps_box::utils {

class file_descriptor_closed_exception : public std::runtime_error
{
public:
    file_descriptor_closed_exception();
    file_descriptor_closed_exception(const file_descriptor_closed_exception &) = default;
    file_descriptor_closed_exception(file_descriptor_closed_exception &&) noexcept = default;
    auto operator=(const file_descriptor_closed_exception &)
            -> file_descriptor_closed_exception & = default;
    auto operator=(file_descriptor_closed_exception &&) noexcept
            -> file_descriptor_closed_exception & = default;
    ~file_descriptor_closed_exception() noexcept override;
};

class file_descriptor_invalid_exception : public std::runtime_error
{
public:
    explicit file_descriptor_invalid_exception(const std::string &message);
    file_descriptor_invalid_exception(const file_descriptor_invalid_exception &) = default;
    file_descriptor_invalid_exception(file_descriptor_invalid_exception &&) noexcept = default;
    auto operator=(const file_descriptor_invalid_exception &)
            -> file_descriptor_invalid_exception & = default;
    auto operator=(file_descriptor_invalid_exception &&) noexcept
            -> file_descriptor_invalid_exception & = default;
    ~file_descriptor_invalid_exception() noexcept override;
};

class file_descriptor
{
public:
    enum class IOStatus : uint8_t { Success, TryAgain, Eof, Closed };

    file_descriptor() = default;
    explicit file_descriptor(int fd, bool auto_close = true);

    virtual ~file_descriptor();

    file_descriptor(const file_descriptor &) = delete;
    auto operator=(const file_descriptor &) -> file_descriptor & = delete;

    file_descriptor(file_descriptor &&other) noexcept;
    auto operator=(file_descriptor &&other) noexcept -> file_descriptor &;

    [[nodiscard]] auto get() const & noexcept -> int;

    [[nodiscard]] auto get() && noexcept -> int;

    [[nodiscard]] auto valid() const -> bool { return fd_ != -1; }

    auto release() -> void;

    [[nodiscard]] auto duplicate() const -> file_descriptor;

    auto duplicate_to(int target, int flags) const -> void;

    auto operator<<(const std::byte &byte) -> file_descriptor &;

    auto operator>>(std::byte &byte) -> file_descriptor &;

    [[nodiscard]] auto proc_path() const -> std::filesystem::path;

    [[nodiscard]] auto current_path() const noexcept -> std::filesystem::path;

    static auto cwd() -> file_descriptor;

    [[nodiscard]] auto type() const -> std::filesystem::file_type;

    auto set_nonblock(bool nonblock) -> void;

    auto read_span(span<std::byte> ws, std::size_t &bytes_read) const -> IOStatus;

    auto write_span(span<const std::byte> rs, std::size_t &bytes_written) const -> IOStatus;

    auto read_vecs(span<struct iovec> ws, std::size_t &bytes_read) const -> IOStatus;

    auto write_vecs(span<const struct iovec> rs, std::size_t &bytes_written) const -> IOStatus;

    template<typename T>
    [[nodiscard]] auto read(T &out) const -> IOStatus
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for raw read");

        std::size_t bytes_read{ 0 };
        auto ws = span<std::byte>(reinterpret_cast<std::byte *>(&out), sizeof(T));
        return read_span(ws, bytes_read);
    }

    template<typename T>
    [[nodiscard]] auto write(const T &in) const -> IOStatus
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        std::size_t bytes_written{ 0 };
        auto rs = span<const std::byte>(reinterpret_cast<const std::byte *>(&in), sizeof(T));
        return write_span(rs, bytes_written);
    }

private:
    // keep this layout, for padding optimization
    bool nonblock_{ false };
    bool auto_close_{ false };
    int fd_{ -1 };
};

} // namespace linyaps_box::utils
