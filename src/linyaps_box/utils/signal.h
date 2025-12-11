// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

#include <csignal> // IWYU pragma: keep

namespace linyaps_box::utils {

auto sigfillset(sigset_t &set) -> void;

auto sigismember(const sigset_t &set, int signo) -> bool;

auto sigprocmask(int how, const sigset_t &new_set, sigset_t *old_set) -> void;

auto sigaction(int sig, const struct sigaction &new_act, struct sigaction *old_act) -> void;

auto reset_signals(const sigset_t &set) -> void;

auto create_signalfd(sigset_t &set, bool nonblock = true) -> file_descriptor;

} // namespace linyaps_box::utils
