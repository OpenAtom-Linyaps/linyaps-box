// SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/utils/atomic_write.h"

#include "linyaps_box/utils/defer.h"
#include "linyaps_box/utils/log.h"

#include <fstream>
#include <system_error>

void linyaps_box::utils::atomic_write(const std::filesystem::path &path, std::string_view content)
{
    auto temp_path = path;
    temp_path += ".tmp";

    auto remove_if_failed = utils::make_errdefer([&temp_path]() noexcept {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        if (ec) {
            LINYAPS_BOX_ERR() << "Failed to remove temporary file " << temp_path << ": "
                              << ec.message();
        }
    });

    std::ofstream temp_file;
    temp_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    temp_file.open(temp_path);
    temp_file << content;
    temp_file.close();
    std::filesystem::rename(temp_path, path);
}
