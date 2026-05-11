// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/atomic_write.h"

#include <fstream>

void linyaps_box::utils::atomic_write(const std::filesystem::path &path, std::string_view content)
{
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";
    std::ofstream temp_file(temp_path);
    if (!temp_file.is_open()) {
        throw std::runtime_error("failed to open temporary file");
    }
    temp_file << content;
    temp_file.close();

    std::filesystem::rename(temp_path, path);
}
