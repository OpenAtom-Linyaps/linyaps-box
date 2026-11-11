// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/oci_config.h"

#include "linyaps_box/io/stream.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/semver.h"
#include "linyaps_box/utils/utils.h"

#include <fmt/std.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

namespace {

void check_oci_version(const json::detail::value_view &val)
{
    if (UNLIKELY(!val.is_string())) {
        val.raise_expected("string");
    }

    try {
        const auto semver = linyaps_box::utils::semver(val.as_string_view());
        if (UNLIKELY(!linyaps_box::utils::semver(oci_config::version).is_compatible_with(semver))) {
            throw json::decode_error{ "unsupported OCI version: " + semver.to_string() };
        }
    } catch (const linyaps_box::utils::invalid_semver &e) {
        throw json::decode_error{ e.what() };
    }
}

} // namespace

LINYAPS_BOX_CONFIG_SCHEMA(oci_config,
                          oci_config_schema,
                          "oci_config",
                          check<&check_oci_version>("ociVersion").required(),
                          field<&oci_config::process_>("process").ignore_null(),
                          field<&oci_config::hostname>("hostname").ignore_null(),
                          field<&oci_config::domainname>("domainname").ignore_null(),
                          field<&oci_config::linux_>("linux").ignore_null(),
                          field<&oci_config::hooks_>("hooks").ignore_null(),
                          field<&oci_config::mounts>("mounts").ignore_null(),
                          field<&oci_config::root_>("root").ignore_null(),
                          field<&oci_config::annotations>("annotations").ignore_null())

void validate(const oci_config &v)
{
    if (v.process_) {
        validate(*v.process_);
    }

    if (v.linux_) {
        validate(*v.linux_);
    }

    if (v.root_) {
        validate(*v.root_);
    }

    if (v.hooks_) {
        validate(*v.hooks_);
    }

    if (v.mounts) {
        for (const auto &m : *v.mounts) {
            validate(m);
        }
    }

    if (!v.annotations) {
        return;
    }

    std::for_each(v.annotations->cbegin(), v.annotations->cend(), [](const auto &pair) {
        if (UNLIKELY(pair.first.empty())) {
            throw std::runtime_error("annotations keys must not be empty");
        }
    });
}

auto oci_config::parse(std::string_view content) -> oci_config
{
    auto config = json::parse_as<oci_config>(content);
    validate(config);
    return config;
}

auto oci_config::load(const std::filesystem::path &path) -> oci_config
{
    const auto content =
      os::throw_if_error(io::read_all(path), fmt::format("failed to read config file: {}", path));
    return parse(std::string_view{ content });
}

} // namespace linyaps_box::config
