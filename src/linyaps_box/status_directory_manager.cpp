// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/status_directory_manager.h"

#include "linyaps_box/utils/log.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace linyaps_box {

namespace {

auto is_valid_id_char(char c) noexcept -> bool
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
      || c == '+' || c == '-' || c == '.';
}

} // namespace

status_directory_manager::status_directory_manager(std::filesystem::path root)
    : root_(std::move(root))
{
    if (std::filesystem::is_directory(root_) || std::filesystem::create_directories(root_)) {
        return;
    }

    throw std::runtime_error("failed to create status directory root: " + root_.string());
}

auto status_directory_manager::list() const -> std::vector<std::string>
{
    std::vector<std::string> ret;
    for (const auto &entry : std::filesystem::directory_iterator(root_)) {
        if (!entry.is_directory()) {
            continue;
        }

        auto id = entry.path().filename().string();
        auto status_file = entry.path() / "status.json";
        if (!std::filesystem::exists(status_file)) {
            LINYAPS_BOX_WARNING() << "Skip " << entry.path() << ": no status.json";
            continue;
        }

        ret.push_back(std::move(id));
    }

    return ret;
}

auto status_directory_manager::validate_id(std::string_view id) -> void
{
    if (id.empty()) {
        throw std::invalid_argument("container ID must not be empty");
    }

    if (id.front() == '.') {
        throw std::invalid_argument("invalid container ID: " + std::string(id));
    }

    if (!std::all_of(id.begin(), id.end(), is_valid_id_char)) {
        throw std::invalid_argument("invalid container ID: " + std::string(id));
    }
}

auto status_directory_manager::get(std::string_view id) const -> status_directory
{
    validate_id(id);
    return status_directory(root_ / id);
}

} // namespace linyaps_box
