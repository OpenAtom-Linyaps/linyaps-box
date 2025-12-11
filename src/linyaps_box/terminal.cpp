// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/terminal.h"

#include "linyaps_box/utils/file.h"
#include "linyaps_box/utils/ioctl.h"
#include "linyaps_box/utils/log.h"
#include "linyaps_box/utils/terminal.h"

#include <utility>

#include <fcntl.h>

namespace linyaps_box {

auto create_pty_pair() -> std::pair<terminal_master, terminal_slave>
{
    // let the container process control the terminal instead of OCI Runtime
    auto master = utils::open("/dev/ptmx", O_RDWR | O_CLOEXEC | O_NOCTTY);

    auto pts = utils::ptsname(master);
    unlockpt(master);
    auto slave = utils::open(pts, O_RDWR | O_CLOEXEC);

    return { terminal_master{ std::move(master) }, terminal_slave{ std::move(slave) } };
}

auto terminal_master::resize(struct winsize size) -> void
{
    utils::ioctl(master_, TIOCSWINSZ, &size);
}

terminal_slave::terminal_slave(terminal_slave &&other) noexcept
    : termios(std::exchange(other.termios, std::nullopt))
    , slave_(std::move(other.slave_))
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
    LINYAPS_BOX_DEBUG() << "Setup stdio";
    slave_.duplicate_to(utils::fileno(stdin), 0);
    slave_.duplicate_to(utils::fileno(stdout), 0);
    slave_.duplicate_to(utils::fileno(stderr), 0);
    utils::ioctl(slave_, TIOCSCTTY, 0);
}

auto terminal_slave::set_size(struct winsize size) -> void
{
    utils::ioctl(slave_, TIOCSWINSZ, &size);
}

auto terminal_slave::set_raw() -> void
{
    if (termios) {
        return;
    }

    LINYAPS_BOX_DEBUG() << "Set terminal " << slave_.get() << " to raw mode";

    struct termios orig_term{};
    utils::tcgetattr(slave_, orig_term);

    auto raw = orig_term;
    ::cfmakeraw(&raw);

    utils::tcsetattr(slave_, TCSANOW, raw);

    termios = orig_term;
}

auto terminal_slave::get_size() -> struct winsize
{
    struct winsize size{};
    utils::ioctl(slave_, TIOCGWINSZ, &size);
    return size;
}

terminal_slave::~terminal_slave() noexcept

try {
    if (termios && slave_.valid()) {
        utils::tcsetattr(slave_, TCSANOW, termios.value());
    }
} catch (std::system_error &e) {
    LINYAPS_BOX_ERR() << "Failed to restore terminal:" << e.what();
} catch (std::exception &e) {
    LINYAPS_BOX_ERR() << "Failed to restore terminal:" << e.what();
} catch (...) {
    LINYAPS_BOX_ERR() << "Failed to restore terminal: unknown exception";
}

} // namespace linyaps_box
