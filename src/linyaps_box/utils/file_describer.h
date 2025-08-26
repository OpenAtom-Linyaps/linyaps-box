// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>

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

class file_descriptor
{
public:
    explicit file_descriptor(int fd = -1);

    ~file_descriptor();

    file_descriptor(const file_descriptor &) = delete;
    auto operator=(const file_descriptor &) -> file_descriptor & = delete;

    file_descriptor(file_descriptor &&other) noexcept;

    auto operator=(file_descriptor &&other) noexcept -> file_descriptor &;

    [[nodiscard]] auto get() const noexcept -> int;

    auto release() && noexcept -> int;

    [[nodiscard]] auto duplicate() const -> file_descriptor;

    auto operator<<(const std::byte &byte) -> file_descriptor &;

    auto operator>>(std::byte &byte) -> file_descriptor &;

    [[nodiscard]] auto proc_path() const -> std::filesystem::path;

    [[nodiscard]] auto current_path() const noexcept -> std::filesystem::path;

private:
    int fd_{ -1 };
};

} // namespace linyaps_box::utils
