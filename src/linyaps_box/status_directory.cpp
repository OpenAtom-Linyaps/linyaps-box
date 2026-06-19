// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/status_directory.h"

#include "linyaps_box/utils/atomic_write.h"
#include "linyaps_box/utils/log.h"
#include "nlohmann/json.hpp"

#include <csignal> // IWYU pragma: keep
#include <fstream>
#include <utility>

#include <unistd.h>

namespace {

auto read_status(const std::filesystem::path &path) -> linyaps_box::container_status_t
{
    nlohmann::json j;
    {
        std::ifstream istrm(path);
        if (istrm.fail()) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "failed to open status file: " + path.string());
        }

        istrm >> j;
    }

    linyaps_box::container_status_t ret{ };

    j.at("ociVersion").get_to(ret.oci_version);
    j.at("pid").get_to(ret.PID);
    j.at("id").get_to(ret.ID);
    ret.status = linyaps_box::from_string_view(j.at("status").get<std::string_view>());
    if (::kill(ret.PID, 0) != 0) {
        if (errno == ESRCH) {
            ret.status = linyaps_box::container_status_t::runtime_status::STOPPED;
        } else if (errno != EPERM) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    "kill(" + std::to_string(ret.PID) + ", 0)");
        }
        // EPERM: process exists but we lack permission, keep status from JSON
    }

    j.at("bundle").get_to(ret.bundle);
    j.at("created").get_to(ret.created);
    j.at("owner").get_to(ret.owner);
    j.at("annotations").get_to(ret.annotations);

    return ret;
}

} // namespace

linyaps_box::status_directory::status_directory(std::filesystem::path path)
    : path_(std::move(path))
{
    if (std::filesystem::is_directory(path_) || std::filesystem::create_directories(path_)) {
        return;
    }

    throw std::runtime_error("failed to create status directory: " + path_.string());
}

void linyaps_box::status_directory::write(const container_status_t &status) const
{
    auto j = nlohmann::json::object({ { "id", status.ID },
                                      { "pid", status.PID },
                                      { "status", to_string_view(status.status) },
                                      { "bundle", status.bundle },
                                      { "created", status.created },
                                      { "owner", status.owner },
                                      { "annotations", status.annotations },
                                      { "ociVersion", status.oci_version } });

    utils::atomic_write(path_ / "status.json", j.dump());
}

auto linyaps_box::status_directory::read() const -> container_status_t
{
    return read_status(path_ / "status.json");
}

void linyaps_box::status_directory::remove() const
{
    LINYAPS_BOX_DEBUG() << "Remove " << path_;
    if (!std::filesystem::exists(path_)) {
        LINYAPS_BOX_WARNING() << "Status directory " << path_ << " does not exist";
        return;
    }

    std::filesystem::remove_all(path_);
}

auto linyaps_box::status_directory::write_config(std::string_view config) const -> void
{
    utils::atomic_write(path_ / "config.json", config);
}

auto linyaps_box::status_directory::save_config(const std::filesystem::path &src) const -> void
{
    std::filesystem::copy(src, path_ / "config.json");
}

auto linyaps_box::status_directory::config() const -> std::filesystem::path
{
    return path_ / "config.json";
}
