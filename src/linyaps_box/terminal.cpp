// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/terminal.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/os/process.h"
#include "linyaps_box/os/pty.h"
#include "linyaps_box/os/termios.h"

// for NAME_MAX
#include <climits> // IWYU pragma: keep

namespace linyaps_box {

auto create_pty_pair() -> pty_data
{
    // let the container process control the terminal instead of OCI Runtime
    auto master = linyaps_box::os::throw_if_error(linyaps_box::os::open(
      "/dev/ptmx",
      { linyaps_box::os::sys::open_flag::cloexec | linyaps_box::os::sys::open_flag::no_ctty,
        linyaps_box::os::sys::access_mode::read_write }));

    constexpr auto max_len = std::string_view::traits_type::length("/dev/pts/") + NAME_MAX + 1;
    std::array<char, max_len> buf; // NOLINT
    auto name = os::throw_if_error(os::ptsname(master, buf), "failed to get pty path");
    os::throw_if_error(os::unlockpt(master), "failed to unlock pty");

    auto pts = std::filesystem::path{ name };
    auto slave = linyaps_box::os::throw_if_error(linyaps_box::os::open(
      pts,
      { linyaps_box::os::sys::open_flag::cloexec, linyaps_box::os::sys::access_mode::read_write }));

    return {
        terminal_slave{ std::move(slave) },
        std::move(pts),
        terminal_master{ std::move(master) },
    };
}

auto terminal_master::resize(struct winsize size) -> void
{
    os::throw_if_error(os::tcsetwinsize(master_, size), "failed to resize terminal");
}

terminal_slave::terminal_slave(terminal_slave &&other) noexcept
    : slave_(std::move(other.slave_))
    , termios(std::exchange(other.termios, std::nullopt))
{
}

terminal_slave &terminal_slave::operator=(terminal_slave &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    termios = std::exchange(other.termios, std::nullopt);
    slave_ = std::move(other.slave_);

    return *this;
}

auto terminal_slave::setup_stdio() -> void
{
    LINYAPS_BOX_LOG_DEBUG("Setup stdio");
    slave_.duplicate_to(STDIN_FILENO, 0);
    slave_.duplicate_to(STDOUT_FILENO, 0);
    slave_.duplicate_to(STDERR_FILENO, 0);
    std::ignore = os::set_control_terminal(slave_);
}

auto terminal_slave::set_size(struct winsize size) -> void
{
    if (size.ws_col == 0 || size.ws_row == 0) {
        auto default_tty = linyaps_box::os::throw_if_error(
          linyaps_box::os::open("/dev/tty",
                                { linyaps_box::os::sys::open_flag::cloexec,
                                  linyaps_box::os::sys::access_mode::read_write }));
        size = os::throw_if_error(os::tcgetwinsize(default_tty));
    }

    os::throw_if_error(os::tcsetwinsize(slave_, size));
}

auto terminal_slave::set_raw() -> void
{
    if (termios) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Set terminal {} to raw mode", slave_.get());

    auto orig_term = os::throw_if_error(os::tcgetattr(slave_), "failed to get original termios");

    auto raw = orig_term;
    ::cfmakeraw(&raw);

    os::throw_if_error(os::tcsetattr(slave_, os::optional_action::now, raw),
                       "failed to set raw mode to slave pty");

    termios = orig_term;
}

auto terminal_slave::get_size() -> struct winsize
{
    return os::throw_if_error(os::tcgetwinsize(slave_));
}

terminal_slave::~terminal_slave() noexcept

try {
    if (termios && slave_.valid()) {
        os::tcsetattr(slave_, os::optional_action::now, termios.value());
    }
} catch (std::exception &e) {
    LINYAPS_BOX_LOG_ERROR("Failed to restore terminal:{}", e.what());
} catch (...) {
    LINYAPS_BOX_LOG_ERROR("Failed to restore terminal: unknown exception");
}

} // namespace linyaps_box
