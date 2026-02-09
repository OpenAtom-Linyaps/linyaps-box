// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include <sys/socket.h>
#include <sys/un.h>

namespace linyaps_box::utils {

auto socketpair(int domain, int type, int protocol) -> std::pair<file_descriptor, file_descriptor>;

auto socket(int domain, int type, int protocol) -> file_descriptor;

auto connect(const file_descriptor &fd, struct sockaddr *addr, socklen_t addrlen) -> void;

} // namespace linyaps_box::utils
