// SPDX-FileCopyrightText: 2022-2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/impl/status_directory.h"

#include "linyaps_box/container_status.h"
#include "linyaps_box/utils/atomic_write.h"
#include "linyaps_box/utils/log.h"
#include "nlohmann/json.hpp"

#include <csignal>
#include <cstdlib>
#include <fstream>

#include <unistd.h>

namespace {

linyaps_box::container_status_t read_status(const std::filesystem::path &path)
{
    nlohmann::json j;
    {
        std::ifstream istrm(path);
        if (istrm.fail()) {
            throw std::runtime_error("failed to open status file:" + path.string());
        }

        istrm >> j;
    }
    linyaps_box::container_status_t ret{};

    ret.oci_version = j["ociVersion"];
    ret.PID = j["pid"];
    ret.ID = j["id"];
    ret.status = linyaps_box::from_string(j["status"].get<std::string>());
    if (kill(ret.PID, 0) != 0) {
        ret.status = linyaps_box::container_status_t::runtime_status::STOPPED;
    }
    ret.bundle = std::string(j["bundle"]);
    ret.created = j["created"];
    ret.owner = j["owner"];
    ret.annotations = j["annotations"];

    return ret;
}

} // namespace

void linyaps_box::impl::status_directory::write(const container_status_t &status) const
{
    nlohmann::json j = nlohmann::json::object({ { "id", status.ID },
                                                { "pid", status.PID },
                                                { "status", to_string(status.status) },
                                                { "bundle", status.bundle },
                                                { "created", status.created },
                                                { "owner", status.owner },
                                                { "annotations", status.annotations },
                                                { "ociVersion", status.oci_version } });

    utils::atomic_write(this->path / (status.ID + ".json"), j.dump());
}

linyaps_box::container_status_t
linyaps_box::impl::status_directory::read(const std::string &id) const
{
    return read_status(this->path / (id + ".json"));
}

void linyaps_box::impl::status_directory::remove(const std::string &id) const
{
    auto path = this->path / (id + ".json");
    LINYAPS_BOX_DEBUG() << "Remove " << path;
    if (!std::filesystem::remove(path)) {
        LINYAPS_BOX_WARNING() << "Failed to remove " << path;
    }
}

std::vector<std::string> linyaps_box::impl::status_directory::list() const
{
    std::vector<std::string> ret;
    for (const auto &entry : std::filesystem::directory_iterator(this->path)) {
        try {
            if (entry.is_regular_file() && entry.path().extension() != ".json") {
                throw std::runtime_error("invalid extension");
            }
            auto status = read_status(entry);
            ret.push_back(status.ID);
        } catch (const std::exception &e) {
            LINYAPS_BOX_WARNING() << "Skip " << entry.path() << ": " << e.what();
            continue;
        }
    }

    return ret;
}

linyaps_box::impl::status_directory::status_directory(const std::filesystem::path &path)
    : path(path)
{
    if (std::filesystem::is_directory(path) || std::filesystem::create_directories(path)) {
        return;
    }

    throw std::runtime_error("failed to create status directory");
}
