// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/file_describer.h"

#include <sys/socket.h>
#include <sys/un.h>

namespace linyaps_box::utils {

auto socketpair(int domain, int type, int protocol) -> std::pair<file_descriptor, file_descriptor>;

} // namespace linyaps_box::utils
