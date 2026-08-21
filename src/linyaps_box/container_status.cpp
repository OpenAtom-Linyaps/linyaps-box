// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/container_status.h"

#include "linyaps_box/utils/process_stat.h"
#include "linyaps_box/utils/time.h"
#include "linyaps_box/utils/utils.h"

#include <csignal> // IWYU pragma: keep
#include <system_error>

#include <unistd.h>

namespace linyaps_box {

auto to_json(nlohmann::json &j, const container_status &s) -> void
{
    std::array<char, utils::max_created_time_len> created_buf{ };
    auto len =
      linyaps_box::utils::to_created_time(linyaps_box::utils::span{ created_buf }, s.created);
    j = nlohmann::json::object({ { "id", s.id },
                                 { "pid", s.pid },
                                 { "process-start-time", s.process_start_time },
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
    j.at("process-start-time").get_to(s.process_start_time);
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

    if (::kill(s.pid, 0) != 0) {
        if (errno == ESRCH) {
            return runtime_status::STOPPED;
        }

        if (UNLIKELY(errno != EPERM)) {
            throw std::system_error(errno,
                                    std::system_category(),
                                    fmt::format("failed to detect process {}", s.pid));
        }

        // EPERM: process exists but we lack permission.
        // Without /proc access we cannot verify the start time, so assume RUNNING.
        return runtime_status::RUNNING;
    }

    // Process is alive. Verify it is the same process (not a recycled PID).
    auto actual = utils::read_process_start_time(s.pid);
    if (!actual) {
        // assume it running
        return runtime_status::RUNNING;
    }

    if (*actual != s.process_start_time) {
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

} // namespace linyaps_box
