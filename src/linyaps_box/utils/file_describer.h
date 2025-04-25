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
};

class file_descriptor
{
public:
    explicit file_descriptor(int fd = -1);

    ~file_descriptor();

    file_descriptor(const file_descriptor &) = delete;
    file_descriptor &operator=(const file_descriptor &) = delete;

    file_descriptor(file_descriptor &&other) noexcept;

    file_descriptor &operator=(file_descriptor &&other) noexcept;

    [[nodiscard]] int get() const noexcept;

    int release() && noexcept;

    [[nodiscard]] file_descriptor duplicate() const;

    file_descriptor &operator<<(const std::byte &byte);

    file_descriptor &operator>>(std::byte &byte);

    [[nodiscard]] std::filesystem::path proc_path() const;

    [[nodiscard]] std::filesystem::path current_path() const noexcept;

private:
    int fd{ -1 };
};

} // namespace linyaps_box::utils
