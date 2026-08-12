// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/terminal.h"

#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/fs.h"
#include "linyaps_box/utils/ioctl.h"
#include "linyaps_box/utils/terminal.h"

#include <utility>

#include <fcntl.h>

namespace linyaps_box {

auto create_pty_pair() -> pty_data
{
    // let the container process control the terminal instead of OCI Runtime
    auto master = linyaps_box::os::throw_if_error(linyaps_box::os::open(
      "/dev/ptmx",
      { linyaps_box::os::sys::open_flag::cloexec | linyaps_box::os::sys::open_flag::no_ctty,
        linyaps_box::os::sys::access_mode::read_write }));

    auto pts = utils::ptsname(master);
    unlockpt(master);
    auto slave = linyaps_box::os::throw_if_error(linyaps_box::os::open(
      pts,
      { linyaps_box::os::sys::open_flag::cloexec, linyaps_box::os::sys::access_mode::read_write }));

    return {
        terminal_slave{ std::move(slave) },
        pts,
        terminal_master{ std::move(master) },
    };
}

auto terminal_master::resize(struct winsize size) -> void
{
    std::ignore = utils::ioctl(master_, TIOCSWINSZ, &size);
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
    std::ignore = utils::ioctl(slave_, TIOCSCTTY, 0);
}

auto terminal_slave::set_size(struct winsize size) -> void
{
    if (size.ws_col == 0 || size.ws_row == 0) {
        auto default_tty = linyaps_box::os::throw_if_error(
          linyaps_box::os::open("/dev/tty",
                                { linyaps_box::os::sys::open_flag::cloexec,
                                  linyaps_box::os::sys::access_mode::read_write }));
        std::ignore = utils::ioctl(default_tty, TIOCGWINSZ, &size);
    }

    std::ignore = utils::ioctl(slave_, TIOCSWINSZ, &size);
}

auto terminal_slave::set_raw() -> void
{
    if (termios) {
        return;
    }

    LINYAPS_BOX_LOG_DEBUG("Set terminal {} to raw mode", slave_.get());

    struct termios orig_term{ };
    utils::tcgetattr(slave_, orig_term);

    auto raw = orig_term;
    ::cfmakeraw(&raw);

    utils::tcsetattr(slave_, TCSANOW, raw);

    termios = orig_term;
}

auto terminal_slave::get_size() -> struct winsize
{
    struct winsize size{ };
    std::ignore = utils::ioctl(slave_, TIOCGWINSZ, &size);
    return size;
}

terminal_slave::~terminal_slave() noexcept

try {
    if (termios && slave_.valid()) {
        utils::tcsetattr(slave_, TCSANOW, termios.value());
    }
} catch (std::exception &e) {
    LINYAPS_BOX_LOG_ERROR("Failed to restore terminal:{}", e.what());
} catch (...) {
    LINYAPS_BOX_LOG_ERROR("Failed to restore terminal: unknown exception");
}

} // namespace linyaps_box
