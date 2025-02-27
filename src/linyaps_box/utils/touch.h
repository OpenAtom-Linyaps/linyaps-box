// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/file_describer.h"

namespace linyaps_box::utils {

file_descriptor touch(const file_descriptor &root, const std::filesystem::path &path);

} // namespace linyaps_box::utils
