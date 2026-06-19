// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_status.h"

namespace linyaps_box {
auto to_string_view(linyaps_box::container_status_t::runtime_status status) -> std::string_view
{
    switch (status) {
    case linyaps_box::container_status_t::runtime_status::CREATING:
        return "creating";
    case linyaps_box::container_status_t::runtime_status::CREATED:
        return "created";
    case linyaps_box::container_status_t::runtime_status::RUNNING:
        return "running";
    case linyaps_box::container_status_t::runtime_status::STOPPED:
        return "stopped";
    default:
        throw std::logic_error("unknown status");
    }
}

auto from_string_view(std::string_view status) -> linyaps_box::container_status_t::runtime_status
{
    if (status == "creating") {
        return linyaps_box::container_status_t::runtime_status::CREATING;
    }
    if (status == "created") {
        return linyaps_box::container_status_t::runtime_status::CREATED;
    }
    if (status == "running") {
        return linyaps_box::container_status_t::runtime_status::RUNNING;
    }
    if (status == "stopped") {
        return linyaps_box::container_status_t::runtime_status::STOPPED;
    }

    throw std::logic_error("unknown status");
}

auto status_to_json(const linyaps_box::container_status_t &status) -> nlohmann::json
{
    return nlohmann::json::object({ { "id", status.ID },
                                    { "pid", status.PID },
                                    { "status", to_string_view(status.status) },
                                    { "bundle", status.bundle.string() },
                                    { "created", status.created },
                                    { "owner", status.owner },
                                    { "annotations", status.annotations },
                                    { "ociVersion", status.oci_version } });
}

} // namespace linyaps_box
