// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>

namespace linyaps_box::utils {

void atomic_write(const std::filesystem::path &path, const std::string &content);

} // namespace linyaps_box::utils
