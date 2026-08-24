// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <sys/ioctl.h>

#include <optional>

#include <termios.h>

namespace linyaps_box {

class terminal_master
{
public:
    explicit terminal_master(utils::file_descriptor master)
        : master_(std::move(master)) { };

    ~terminal_master() noexcept = default;

    terminal_master(terminal_master &&) noexcept = default;
    terminal_master &operator=(terminal_master &&) noexcept = default;

    terminal_master(const terminal_master &) = delete;
    terminal_master &operator=(const terminal_master &) = delete;

    [[nodiscard]] auto take() && -> utils::file_descriptor { return std::move(master_); }

    [[nodiscard]] auto fd() const & noexcept -> const utils::file_descriptor & { return master_; }

    [[nodiscard]] auto fd() & noexcept -> utils::file_descriptor & { return master_; }

    auto resize(struct winsize size) -> void;

private:
    utils::file_descriptor master_;
};

class terminal_slave
{
public:
    explicit terminal_slave(utils::file_descriptor slave)
        : slave_(std::move(slave)) { };

    ~terminal_slave() noexcept;

    terminal_slave(terminal_slave &&) noexcept;
    terminal_slave &operator=(terminal_slave &&) noexcept;

    terminal_slave(const terminal_slave &) = delete;
    terminal_slave &operator=(const terminal_slave &) = delete;

    auto setup_stdio() -> void;

    auto set_size(struct winsize size) -> void;

    auto get_size() -> struct winsize;

    [[nodiscard]] auto take() && -> utils::file_descriptor { return std::move(slave_); }

    [[nodiscard]] auto fd() const & noexcept -> const utils::file_descriptor & { return slave_; }

    auto set_raw() -> void;

private:
    utils::file_descriptor slave_;
    std::optional<struct termios> termios;
};

struct pty_data
{
    terminal_slave slave;
    std::filesystem::path path;
    terminal_master master;
};

[[nodiscard]] auto create_pty_pair() -> pty_data;

} // namespace linyaps_box
