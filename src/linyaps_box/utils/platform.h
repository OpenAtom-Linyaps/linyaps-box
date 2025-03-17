// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include <string_view>

namespace linyaps_box::utils {
int str_to_signal(std::string_view str);
int str_to_rlimit(std::string_view str);
} // namespace linyaps_box::utils
