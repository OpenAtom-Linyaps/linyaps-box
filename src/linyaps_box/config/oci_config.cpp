// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/oci_config.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/utils/semver.h"
#include "linyaps_box/utils/utils.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace linyaps_box::config {

void from_json(const nlohmann::json &j, oci_config &v)
{
    bool have_oci_version{ false };
    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "ociVersion")) {
            auto semver = linyaps_box::utils::semver(val.get_ref<const std::string &>());
            if (UNLIKELY(
                  !linyaps_box::utils::semver(oci_config::version).is_compatible_with(semver))) {
                throw std::runtime_error("unsupported OCI version: " + semver.to_string());
            }

            have_oci_version = true;
        } else if (key_matches(k, "process")) {
            if (!val.is_null()) {
                val.get_to(v.process_.emplace());
            }
        } else if (key_matches(k, "hostname")) {
            if (!val.is_null()) {
                val.get_to(v.hostname.emplace());
            }
        } else if (key_matches(k, "domainname")) {
            if (!val.is_null()) {
                val.get_to(v.domainname.emplace());
            }
        } else if (key_matches(k, "linux")) {
            if (!val.is_null()) {
                val.get_to(v.linux_.emplace());
            }
        } else if (key_matches(k, "hooks")) {
            if (!val.is_null()) {
                val.get_to(v.hooks_.emplace());
            }
        } else if (key_matches(k, "mounts")) {
            if (!val.is_null()) {
                val.get_to(v.mounts);
            }
        } else if (key_matches(k, "root")) {
            if (!val.is_null()) {
                val.get_to(v.root_.emplace());
            }
        } else if (key_matches(k, "annotations")) {
            if (!val.is_null()) {
                val.get_to(v.annotations.emplace());
            }
        }
    }

    if (!have_oci_version) {
        throw std::runtime_error("oci_config.ociVersion is required");
    }
}

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

    for (const auto &m : v.mounts) {
        validate(m);
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
    auto config = nlohmann::json::parse(content).get<oci_config>();
    validate(config);
    return config;
}

auto oci_config::parse(const std::filesystem::path &path) -> oci_config
{
    utils::uninit_vector<std::byte> buf;
    auto len = read_json_to(path, buf);

    const std::string_view content_view{ reinterpret_cast<const char *>(buf.data()), // NOLINT
                                         len };

    return parse(content_view);
}

} // namespace linyaps_box::config
