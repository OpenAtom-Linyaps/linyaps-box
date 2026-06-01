// SPDX-FileCopyrightText: 2022-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/span.h"

#include <filesystem>

#include <fcntl.h>
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

enum class IOStatus : uint8_t { Success, TryAgain, Eof, Closed, Timeout };

struct IOResult
{
    IOStatus status;
    std::size_t bytes;
};

class file_descriptor
{
public:
    file_descriptor() = default;
    explicit file_descriptor(int fd, bool auto_close = true);

    ~file_descriptor() noexcept;

    file_descriptor(const file_descriptor &) = delete;
    auto operator=(const file_descriptor &) -> file_descriptor & = delete;

    file_descriptor(file_descriptor &&other) noexcept;
    auto operator=(file_descriptor &&other) noexcept -> file_descriptor &;

    [[nodiscard]] auto get() const & noexcept -> int;

    [[nodiscard]] auto get() && -> int;

    [[nodiscard]] auto valid() const -> bool { return fd_ != -1; }

    [[nodiscard]] auto release() & -> int;

    auto close() & -> void;

    [[nodiscard]] auto duplicate() const -> file_descriptor;

    auto duplicate_to(int target, int flags) const -> void;

    auto operator<<(const std::byte &byte) -> file_descriptor &;

    auto operator>>(std::byte &byte) -> file_descriptor &;

    [[nodiscard]] auto proc_path() const -> std::filesystem::path;

    [[nodiscard]] auto current_path() const -> std::filesystem::path;

    static auto cwd() -> file_descriptor;

    [[nodiscard]] auto type() const -> std::filesystem::file_type;

    [[nodiscard]] auto flags() const -> unsigned int;

    auto set_flags(unsigned int flags) const & -> void;

    auto set_nonblock(bool nonblock) const & -> void;

    [[nodiscard]] auto nonblock() const -> bool;

    [[nodiscard]] auto read_span(span<std::byte> ws) const -> IOResult;

    [[nodiscard]] auto write_span(span<const std::byte> rs) const -> IOResult;

    [[nodiscard]] auto read_vecs(span<struct iovec> ws) const -> IOResult;

    [[nodiscard]] auto write_vecs(span<const struct iovec> rs) const -> IOResult;

    template <typename T>
    [[nodiscard]] auto read(T &out) const -> IOResult
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for raw read");
        auto ws =
          linyaps_box::utils::span<std::byte>(reinterpret_cast<std::byte *>(&out), sizeof(T));

        std::size_t total_bytes{ 0 };
        while (total_bytes < sizeof(T)) {
            auto [status, bytes] = read_span(ws.subspan(total_bytes));
            total_bytes += bytes;

            if (status != IOStatus::Success) {
                return { status, total_bytes };
            }

            if (bytes == 0) {
                return { IOStatus::Eof, total_bytes };
            }
        }

        return { IOStatus::Success, total_bytes };
    }

    template <typename T>
    [[nodiscard]] auto write(const T &in) const -> IOResult
    {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        auto rs = span<const std::byte>(reinterpret_cast<const std::byte *>(&in), sizeof(T));

        std::size_t total_bytes{ 0 };
        while (total_bytes < sizeof(T)) {
            auto [status, bytes] = write_span(rs.subspan(total_bytes));
            total_bytes += bytes;

            if (status != IOStatus::Success) {
                return { status, total_bytes };
            }

            if (bytes == 0) {
                return { IOStatus::TryAgain, total_bytes };
            }
        }

        return { IOStatus::Success, total_bytes };
    }

private:
    // keep this layout, for padding optimization
    int fd_{ -1 };
    bool auto_close_{ false };
};

} // namespace linyaps_box::utils
