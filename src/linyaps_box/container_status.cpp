// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_status.h"

#include "linyaps_box/infra/process_handle.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/utils/date.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace linyaps_box {

namespace {

auto read_created(const json::detail::value_view &val) -> std::chrono::system_clock::time_point
{
    try {
        return utils::from_created_time(val.as_string_view());
    } catch (const std::invalid_argument &e) {
        throw json::decode_error{ e.what() };
    }
}

} // namespace

auto to_json(nlohmann::json &j, const container_status &s) -> void
{
    std::array<char, utils::max_created_time_len> created_buf{ };
    const auto len =
      linyaps_box::utils::to_created_time(linyaps_box::utils::span{ created_buf }, s.created);
    auto obj = nlohmann::json::object({
      { "id", s.id },
      { "pid", s.pid },
      { "bundle", s.bundle.string() },
      { "created", std::string_view{ created_buf.data(), len } },
      { "owner", s.owner },
      { "annotations", s.annotations },
      { "ociVersion", s.oci_version },
    });

    if (s.process_start_time) {
        obj["process-start-time"] = *s.process_start_time;
    }

    j = std::move(obj);
}

auto to_string_view(runtime_status s) noexcept -> std::string_view
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

    if (!s.process_start_time) {
        // Legacy state files lack the process start time, so PID-reuse cannot
        // be detected. Assume RUNNING (optimistic on uncertainty).
        return runtime_status::RUNNING;
    }

    if (stat->start_time != *s.process_start_time) {
        // PID was recycled: the original container process is gone.
        return runtime_status::STOPPED;
    }

    // Without a start-synchronisation primitive we cannot distinguish created
    // from running.
    // A running process is reported as RUNNING; created will be
    // distinguished when the primitive is added alongside separate create/start.
    return runtime_status::RUNNING;
}

auto to_oci_json(const container_status &s, runtime_status status) -> nlohmann::json
{
    auto result = nlohmann::json(s);
    result["status"] = to_string_view(status);
    return result;
}

LINYAPS_BOX_CONFIG_SCHEMA(container_status,
                          container_status_schema,
                          "container_status",
                          field<&container_status::id>("id").required(),
                          field<&container_status::pid>("pid").required(),
                          field<&container_status::process_start_time>("process-start-time"),
                          field<&container_status::bundle>("bundle").required(),
                          field<&container_status::created, &read_created>("created").required(),
                          field<&container_status::owner>("owner").required(),
                          field<&container_status::annotations>("annotations").required(),
                          field<&container_status::oci_version>("ociVersion").required())

} // namespace linyaps_box
