// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

// for struct winsize
#include "linyaps_box/utils/ioctl.h" // IWYU pragma: keep

#include <termios.h>

namespace linyaps_box::utils {

auto ptsname(const file_descriptor &pts) -> std::filesystem::path;

auto unlockpt(const file_descriptor &pt) -> void;

auto tcgetattr(const file_descriptor &fd, struct termios &termios) -> void;

auto tcsetattr(const file_descriptor &fd, int action, const struct termios &termios) -> void;

auto isatty(const file_descriptor &fd) -> bool;

auto fileno(FILE *stream) -> int;

} // namespace linyaps_box::utils
