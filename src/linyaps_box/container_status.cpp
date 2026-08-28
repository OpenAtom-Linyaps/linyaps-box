// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_status.h"

#include "linyaps_box/infra/process_handle.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/utils/time.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

namespace linyaps_box {

auto to_json(nlohmann::json &j, const container_status &s) -> void
{
    std::array<char, utils::max_created_time_len> created_buf{ };
    auto len =
      linyaps_box::utils::to_created_time(linyaps_box::utils::span{ created_buf }, s.created);
    j = nlohmann::json::object({ { "id", s.id },
                                 { "pid", s.pid },
                                 { "process-start-time", s.process_start_time.value_or(0) },
                                 { "bundle", s.bundle.string() },
                                 { "created", std::string_view{ created_buf.data(), len } },
                                 { "owner", s.owner },
                                 { "annotations", s.annotations },
                                 { "ociVersion", s.oci_version } });
}

auto from_json(const nlohmann::json &j, container_status &s) -> void
{
    j.at("id").get_to(s.id);
    j.at("pid").get_to(s.pid);
    if (auto it = j.find("process-start-time"); it != j.end()) {
        s.process_start_time = it->get<std::uint64_t>();
    } else {
        // Legacy status files (2.2.x) predate the start-time field.
        s.process_start_time.reset();
    }
    s.bundle = j.at("bundle").get<std::string>();
    s.created = utils::from_created_time(j.at("created").get_ref<const std::string &>());
    j.at("owner").get_to(s.owner);
    j.at("annotations").get_to(s.annotations);
    j.at("ociVersion").get_to(s.oci_version);
}

auto to_string_view(runtime_status s) -> std::string_view
{
    using namespace std::string_view_literals;
    switch (s) {
    case runtime_status::CREATING:
        return "creating"sv;
    case runtime_status::CREATED:
        return "created"sv;
    case runtime_status::RUNNING:
        return "running"sv;
    case runtime_status::STOPPED:
        return "stopped"sv;
    }

    __builtin_unreachable();
}

auto derive_status(const container_status &s) -> runtime_status
{
    if (s.pid <= 0) {
        return runtime_status::CREATING;
    }

    auto handle = infra::process_handle::open(s.pid);
    if (!handle) {
        if (handle.error() == std::errc::no_such_process) {
            return runtime_status::STOPPED;
        }

        LINYAPS_BOX_LOG_INFO("failed to get container process handle: {}", handle.error());
        // optimistic on uncertainty.
        // EPERM or other errors mean we cannot inspect the process,
        // assume RUNNING rather than reporting a false STOPPED.
        return runtime_status::RUNNING;
    }

    auto stat = handle->status();
    if (!stat) {
        LINYAPS_BOX_LOG_INFO("failed to get container process stat: {}", stat.error());
        // Cannot read the pinned process's stat — assume RUNNING (optimistic).
        return runtime_status::RUNNING;
    }

    if (stat->state == infra::process_state::zombie || stat->state == infra::process_state::dead) {
        return runtime_status::STOPPED;
    }

    if (s.process_start_time && stat->start_time != *s.process_start_time) {
        // PID was recycled: the original container process is gone.
        return runtime_status::STOPPED;
    }

    // Without a start-synchronisation primitive we cannot distinguish created
    // from running.
    // A running process is reported as RUNNING; created will be
    // distinguished when the primitive is added alongside separate create/start.
    return runtime_status::RUNNING;
}

auto to_oci_json(container_status s, runtime_status rs) -> nlohmann::json
{
    std::array<char, utils::max_created_time_len> created_buf{ };
    auto len =
      linyaps_box::utils::to_created_time(linyaps_box::utils::span{ created_buf }, s.created);
    return nlohmann::json::object({ { "id", std::move(s.id) },
                                    { "pid", s.pid },
                                    { "status", to_string_view(rs) },
                                    { "bundle", s.bundle.string() },
                                    { "created", std::string_view{ created_buf.data(), len } },
                                    { "owner", std::move(s.owner) },
                                    { "annotations", std::move(s.annotations) },
                                    { "ociVersion", std::move(s.oci_version) } });
}

namespace detail {

auto format_container_status_json(const container_status &status, bool pretty) -> std::string
{
    auto json = nlohmann::json(status);
    return json.dump(pretty ? 4 : -1);
}

} // namespace detail

} // namespace linyaps_box
