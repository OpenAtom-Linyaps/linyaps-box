// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/utils/time.h"

#include <fmt/format.h>
#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace linyaps_box {

struct container_status
{
    std::unordered_map<std::string, std::string> annotations;
    std::filesystem::path bundle;
    std::string id;
    std::string oci_version;
    std::string owner; // extension field
    std::uint64_t process_start_time;
    std::chrono::system_clock::time_point created; // extension field
    pid_t pid;
};

auto from_json(const nlohmann::json &j, container_status &s) -> void;
auto to_json(nlohmann::json &j, const container_status &s) -> void;

enum class runtime_status : std::uint8_t { CREATING, CREATED, RUNNING, STOPPED };

auto to_string_view(runtime_status s) -> std::string_view;
auto derive_status(const container_status &s) -> runtime_status;

auto to_oci_json(container_status s, runtime_status rs) -> nlohmann::json;

namespace detail {
auto format_container_status_json(const container_status &status, bool pretty) -> std::string;
} // namespace detail

} // namespace linyaps_box

template <>
struct fmt::formatter<linyaps_box::container_status>
{
    enum class Presentation : uint8_t { plaintext, json, pretty_json };
    Presentation presentation{ Presentation::plaintext };

    constexpr auto parse(fmt::format_parse_context &ctx)
    {
        const auto *it = ctx.begin();
        const auto *end = ctx.end();

        if (it == end || *it == '}') {
            return it;
        }

        switch (*it) {
        case 't': {
            presentation = Presentation::plaintext;
        } break;
        case 'j': {
            presentation = Presentation::json;
        } break;
        case 'p': {
            presentation = Presentation::pretty_json;
        } break;
        default:
            throw fmt::format_error("invalid format specifier for linyaps_box::container::status");
        }

        if (it != end && *it != '}') {
            throw fmt::format_error("unexpected trailing characters in format specifier");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const linyaps_box::container_status &status, FormatContext &ctx) const
    {
        if (presentation == Presentation::plaintext) {
            std::array<char, 30> time_buf{ };
            auto len = linyaps_box::utils::to_created_time(linyaps_box::utils::span{ time_buf },
                                                           status.created);
            return fmt::format_to(
              ctx.out(),
              "status{{ ociVersion: {}, id: {}, pid: {}, bundle: {}, annotations: {}, owner: {}, "
              "process_start_time: {}, created: {} }}",
              status.oci_version,
              status.id,
              status.pid,
              status.annotations,
              status.owner,
              status.process_start_time,
              std::string_view{ time_buf.data(), len });
        }

        auto json_str = linyaps_box::detail::format_container_status_json(
          status,
          presentation == Presentation::pretty_json);
        return fmt::format_to(ctx.out(), "{}", json_str);
    }
};
