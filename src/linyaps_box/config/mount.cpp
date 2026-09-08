// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/mount.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/log/macro.h"

#include <fmt/std.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace linyaps_box::config {

namespace {

constexpr auto idmap_options_table = get_enum_table_from<mount::idmap_type>();

enum class option_kind : std::uint8_t { vfs, propagation, rec_attr_set, rec_attr_clr };

struct mount_option
{
    std::string_view name;
    uint32_t value;
    option_kind kind;
    bool clear;
};

struct parsed_mount_options
{
    std::string data;
    std::optional<std::vector<id_mapping>> gid_mappings;
    std::optional<std::vector<id_mapping>> uid_mappings;
    std::optional<mount::recursive_attr> rec_attr;
    utils::bitflags<vfs_flag> vfs_flags;
    std::optional<mount::idmap_type> idmap;
    mount::extension extension_flags{ mount::extension::none };
    utils::bitflags<propagation_flag> propagation_flags;
};

// from https://github.com/opencontainers/runtime-spec/blob/main/config.md#linux-mount-options
constexpr std::array<mount_option, 33 + 8 + 9 + 9> mount_options{ {
  // vfs
  { "bind", static_cast<uint32_t>(vfs_flag::bind), option_kind::vfs, false },
  { "dirsync", static_cast<uint32_t>(vfs_flag::dirsync), option_kind::vfs, false },
  { "defaults", static_cast<uint32_t>(vfs_flag::none), option_kind::vfs, false },
  { "iversion", static_cast<uint32_t>(vfs_flag::iversion), option_kind::vfs, false },
  { "lazytime", static_cast<uint32_t>(vfs_flag::lazytime), option_kind::vfs, false },
  { "mand", static_cast<uint32_t>(vfs_flag::mand), option_kind::vfs, false },
  { "noatime", static_cast<uint32_t>(vfs_flag::noatime), option_kind::vfs, false },
  { "nodev", static_cast<uint32_t>(vfs_flag::nodev), option_kind::vfs, false },
  { "nodiratime", static_cast<uint32_t>(vfs_flag::nodiratime), option_kind::vfs, false },
  { "noexec", static_cast<uint32_t>(vfs_flag::noexec), option_kind::vfs, false },
  { "nosuid", static_cast<uint32_t>(vfs_flag::nosuid), option_kind::vfs, false },
  { "nosymfollow", static_cast<uint32_t>(vfs_flag::nosymfollow), option_kind::vfs, false },
  { "rbind",
    static_cast<uint32_t>((vfs_flag::bind | vfs_flag::rec).to_raw()),
    option_kind::vfs,
    false },
  { "relatime", static_cast<uint32_t>(vfs_flag::relatime), option_kind::vfs, false },
  { "remount", static_cast<uint32_t>(vfs_flag::remount), option_kind::vfs, false },
  { "ro", static_cast<uint32_t>(vfs_flag::ro), option_kind::vfs, false },
  { "silent", static_cast<uint32_t>(vfs_flag::silent), option_kind::vfs, false },
  { "strictatime", static_cast<uint32_t>(vfs_flag::strictatime), option_kind::vfs, false },
  { "sync", static_cast<uint32_t>(vfs_flag::sync), option_kind::vfs, false },
  { "async", static_cast<uint32_t>(vfs_flag::sync), option_kind::vfs, true },
  { "atime", static_cast<uint32_t>(vfs_flag::noatime), option_kind::vfs, true },
  { "dev", static_cast<uint32_t>(vfs_flag::nodev), option_kind::vfs, true },
  { "diratime", static_cast<uint32_t>(vfs_flag::nodiratime), option_kind::vfs, true },
  { "exec", static_cast<uint32_t>(vfs_flag::noexec), option_kind::vfs, true },
  { "loud", static_cast<uint32_t>(vfs_flag::silent), option_kind::vfs, true },
  { "noiversion", static_cast<uint32_t>(vfs_flag::iversion), option_kind::vfs, true },
  { "nolazytime", static_cast<uint32_t>(vfs_flag::lazytime), option_kind::vfs, true },
  { "nomand", static_cast<uint32_t>(vfs_flag::mand), option_kind::vfs, true },
  { "norelatime", static_cast<uint32_t>(vfs_flag::relatime), option_kind::vfs, true },
  { "nostrictatime", static_cast<uint32_t>(vfs_flag::strictatime), option_kind::vfs, true },
  { "rw", static_cast<uint32_t>(vfs_flag::ro), option_kind::vfs, true },
  { "suid", static_cast<uint32_t>(vfs_flag::nosuid), option_kind::vfs, true },
  { "symfollow", static_cast<uint32_t>(vfs_flag::nosymfollow), option_kind::vfs, true },
  // propagation
  { "private", static_cast<uint32_t>(propagation_flag::private_), option_kind::propagation, false },
  { "rprivate",
    static_cast<uint32_t>((propagation_flag::private_ | propagation_flag::rec).to_raw()),
    option_kind::propagation,
    false },
  { "slave", static_cast<uint32_t>(propagation_flag::slave), option_kind::propagation, false },
  { "rslave",
    static_cast<uint32_t>((propagation_flag::slave | propagation_flag::rec).to_raw()),
    option_kind::propagation,
    false },
  { "shared", static_cast<uint32_t>(propagation_flag::shared), option_kind::propagation, false },
  { "rshared",
    static_cast<uint32_t>((propagation_flag::shared | propagation_flag::rec).to_raw()),
    option_kind::propagation,
    false },
  { "unbindable",
    static_cast<uint32_t>(propagation_flag::unbindable),
    option_kind::propagation,
    false },
  { "runbindable",
    static_cast<uint32_t>((propagation_flag::unbindable | propagation_flag::rec).to_raw()),
    option_kind::propagation,
    false },
  // recursive mount_setattr
  { "rro", static_cast<uint32_t>(recursive_attr_flag::rdonly), option_kind::rec_attr_set, false },
  { "rnosuid",
    static_cast<uint32_t>(recursive_attr_flag::nosuid),
    option_kind::rec_attr_set,
    false },
  { "rnodev", static_cast<uint32_t>(recursive_attr_flag::nodev), option_kind::rec_attr_set, false },
  { "rnoexec",
    static_cast<uint32_t>(recursive_attr_flag::noexec),
    option_kind::rec_attr_set,
    false },
  { "rnodiratime",
    static_cast<uint32_t>(recursive_attr_flag::nodiratime),
    option_kind::rec_attr_set,
    false },
  { "rnoatime",
    static_cast<uint32_t>(recursive_attr_flag::noatime),
    option_kind::rec_attr_set,
    false },
  { "rstrictatime",
    static_cast<uint32_t>(recursive_attr_flag::strictatime),
    option_kind::rec_attr_set,
    false },
  { "rnosymfollow",
    static_cast<uint32_t>(recursive_attr_flag::nosymfollow),
    option_kind::rec_attr_set,
    false },
  { "rrelatime", 0U, option_kind::rec_attr_set, false },
  { "rrw", static_cast<uint32_t>(recursive_attr_flag::rdonly), option_kind::rec_attr_clr, false },
  { "rsuid", static_cast<uint32_t>(recursive_attr_flag::nosuid), option_kind::rec_attr_clr, false },
  { "rdev", static_cast<uint32_t>(recursive_attr_flag::nodev), option_kind::rec_attr_clr, false },
  { "rexec", static_cast<uint32_t>(recursive_attr_flag::noexec), option_kind::rec_attr_clr, false },
  { "rdiratime",
    static_cast<uint32_t>(recursive_attr_flag::nodiratime),
    option_kind::rec_attr_clr,
    false },
  { "ratime",
    static_cast<uint32_t>(recursive_attr_flag::noatime),
    option_kind::rec_attr_clr,
    false },
  { "rnostrictatime",
    static_cast<uint32_t>(recursive_attr_flag::strictatime),
    option_kind::rec_attr_clr,
    false },
  { "rsymfollow",
    static_cast<uint32_t>(recursive_attr_flag::nosymfollow),
    option_kind::rec_attr_clr,
    false },
  { "rnorelatime", 0U, option_kind::rec_attr_clr, false },
} };

constexpr auto make_sorted_mount_options() noexcept
  -> std::array<mount_option, mount_options.size()>
{
    auto out = mount_options;
    linyaps_box::utils::detail::shell_sort(
      linyaps_box::utils::span(out),
      [](const mount_option &a, const mount_option &b) noexcept {
          return a.name < b.name;
      });

    return out;
}

constexpr auto sorted_mount_options = make_sorted_mount_options();

[[nodiscard]] auto find_mount_option(std::string_view name) noexcept -> const mount_option *
{
    const auto *const it = std::lower_bound(sorted_mount_options.cbegin(),
                                            sorted_mount_options.cend(),
                                            name,
                                            [](const mount_option &entry, std::string_view n) {
                                                return entry.name < n;
                                            });
    if (it != sorted_mount_options.cend() && it->name == name) {
        return &*it;
    }

    return nullptr;
}

auto parse_id_mapping_chunk(std::string_view chunk) -> id_mapping
{
    // format: "containerID:hostID:size"
    auto first_colon = chunk.find(':');
    if (UNLIKELY(first_colon == std::string_view::npos)) {
        throw std::runtime_error(fmt::format("invalid id mapping: {}", chunk));
    }

    auto second_colon = chunk.find(':', first_colon + 1);
    if (UNLIKELY(second_colon == std::string_view::npos)) {
        throw std::runtime_error(fmt::format("invalid id mapping: {}", chunk));
    }

    id_mapping mapping{ };
    const char *const base = chunk.data();
    auto [p1, ec1] = std::from_chars(base, base + first_colon, mapping.container_id);
    // from_chars may stop early on trailing garbage; the whole field must be consumed.
    if (UNLIKELY(ec1 != std::errc{ } || p1 != base + first_colon)) {
        throw std::runtime_error(fmt::format("invalid container id in mapping: {}", chunk));
    }

    auto [p2, ec2] = std::from_chars(base + first_colon + 1, base + second_colon, mapping.host_id);
    if (UNLIKELY(ec2 != std::errc{ } || p2 != base + second_colon)) {
        throw std::runtime_error(fmt::format("invalid host id in mapping: {}", chunk));
    }

    auto [p3, ec3] = std::from_chars(base + second_colon + 1, base + chunk.size(), mapping.size);
    if (UNLIKELY(ec3 != std::errc{ } || p3 != base + chunk.size())) {
        throw std::runtime_error(fmt::format("invalid size in mapping: {}", chunk));
    }

    return mapping;
}

auto parse_mappings(std::string_view value) -> std::optional<std::vector<id_mapping>>
{
    std::vector<id_mapping> result;
    // Estimate capacity from comma count to avoid realloc during push_back
    auto commas = std::count(value.begin(), value.end(), ',');
    result.reserve(static_cast<std::size_t>(commas) + 1);

    while (!value.empty()) {
        auto comma_pos = value.find(',');
        auto chunk = value.substr(0, comma_pos);
        result.push_back(parse_id_mapping_chunk(chunk));

        if (comma_pos == std::string_view::npos) {
            break;
        }

        value = value.substr(comma_pos + 1);
    }

    return result;
}

struct inline_idmap_result
{
    std::optional<std::vector<id_mapping>> uid_mappings;
    std::optional<std::vector<id_mapping>> gid_mappings;
    mount::idmap_type type;
};

auto parse_inline_idmap_option(std::string_view opt, mount::idmap_type type) -> inline_idmap_result
{
    auto eq_pos = opt.find('=');
    inline_idmap_result result;
    result.type = type;
    auto rest = opt.substr(eq_pos + 1);

    while (!rest.empty()) {
        auto comma_pos = rest.find(',');
        auto part = rest.substr(0, comma_pos);

        auto eq2_pos = part.find('=');
        if (eq2_pos == std::string_view::npos) {
            throw std::runtime_error(fmt::format("invalid id mapping option: {}", opt));
        }

        auto key = part.substr(0, eq2_pos);
        auto value = part.substr(eq2_pos + 1);

        if (key == "uids") {
            result.uid_mappings = parse_mappings(value);
        } else if (key == "gids") {
            result.gid_mappings = parse_mappings(value);
        } else {
            throw std::runtime_error(fmt::format("unknown id mapping key: {}", key));
        }

        if (comma_pos == std::string_view::npos) {
            break;
        }

        rest = rest.substr(comma_pos + 1);
    }

    return result;
}

auto parse_mount_options(const std::vector<std::string_view> &options) -> parsed_mount_options
{
    parsed_mount_options result;

    auto ensure_idmap_compatible = [&result](mount::idmap_type candidate) {
        if (UNLIKELY(result.idmap.has_value() && *result.idmap != candidate)) {
            throw std::runtime_error("idmap and ridmap options are mutually exclusive");
        }
    };

    for (const auto &opt : options) {
        if (const auto *entry = find_mount_option(opt)) {
            switch (entry->kind) {
            case option_kind::vfs: {
                const auto flag = utils::bitflags<vfs_flag>::from_raw_truncate(
                  static_cast<std::uint32_t>(entry->value));
                if (entry->clear) {
                    result.vfs_flags &= ~flag;
                } else {
                    result.vfs_flags |= flag;
                }
            } break;
            case option_kind::propagation: {
                result.propagation_flags |= utils::bitflags<propagation_flag>::from_raw_truncate(
                  static_cast<std::uint8_t>(entry->value));
            } break;
            case option_kind::rec_attr_set: {
                if (!result.rec_attr) {
                    result.rec_attr.emplace();
                }

                result.rec_attr->set |= utils::bitflags<recursive_attr_flag>::from_raw_truncate(
                  static_cast<std::uint8_t>(entry->value));
            } break;
            case option_kind::rec_attr_clr: {
                if (!result.rec_attr) {
                    result.rec_attr.emplace();
                }

                result.rec_attr->clr |= utils::bitflags<recursive_attr_flag>::from_raw_truncate(
                  static_cast<std::uint8_t>(entry->value));
            } break;
            }

            continue;
        }

        if (auto ev = get_enum_table_from<mount::extension>().from_name(opt)) {
            result.extension_flags |= *ev;
            continue;
        }

        // Handle simple "idmap" or "ridmap" flags
        if (auto iv = idmap_options_table.from_name(opt)) {
            ensure_idmap_compatible(*iv);

            result.idmap.emplace(std::move(iv).value());
            continue;
        }

        // Handle inline mapping strings like "idmap=uids=0:1000:1,gids=0:1000:1"
        if (auto eq_pos = opt.find('='); eq_pos != std::string_view::npos) {
            auto prefix = opt.substr(0, eq_pos);
            if (auto iv = idmap_options_table.from_name(prefix)) {
                ensure_idmap_compatible(*iv);

                auto inline_result = parse_inline_idmap_option(opt, *iv);
                result.idmap.emplace(inline_result.type);
                result.uid_mappings = std::move(inline_result.uid_mappings);
                result.gid_mappings = std::move(inline_result.gid_mappings);

                continue;
            }
        }

        if (!result.data.empty()) {
            result.data.push_back(',');
        }

        result.data.append(opt);
    }

    return result;
}

} // namespace

void from_json(const nlohmann::json &j, mount &v)
{
    std::optional<std::vector<id_mapping>> inline_uid_mappings;
    std::optional<std::vector<id_mapping>> inline_gid_mappings;
    bool have_destination{ false };

    for (const auto &[key, val] : j.items()) {
        const auto k = std::string_view{ key };
        if (key_matches(k, "destination")) {
            val.get_to(v.destination);
            have_destination = true;
        } else if (key_matches(k, "source")) {
            if (!val.is_null()) {
                val.get_to(v.source.emplace());
            }
        } else if (key_matches(k, "type")) {
            if (!val.is_null()) {
                val.get_to(v.type.emplace());
            }
        } else if (key_matches(k, "uidMappings")) {
            if (!val.is_null()) {
                val.get_to(v.uid_mappings.emplace());
            }
        } else if (key_matches(k, "gidMappings")) {
            if (!val.is_null()) {
                val.get_to(v.gid_mappings.emplace());
            }
        } else if (key_matches(k, "options")) {
            if (!val.is_null()) {
                std::vector<std::string_view> options;
                options.reserve(val.size());
                std::transform(val.cbegin(),
                               val.cend(),
                               std::back_inserter(options),
                               [](const auto &opt) {
                                   return opt.template get<std::string_view>();
                               });

                auto parsed = parse_mount_options(options);

                v.vfs_flags = parsed.vfs_flags;
                v.propagation_flags = parsed.propagation_flags;
                v.rec_attr = parsed.rec_attr;
                v.extension_flags = parsed.extension_flags;
                v.idmap = parsed.idmap;
                v.data = std::move(parsed.data);

                inline_uid_mappings = std::move(parsed.uid_mappings);
                inline_gid_mappings = std::move(parsed.gid_mappings);
            }
        }
    }

    if (inline_uid_mappings) {
        if (v.uid_mappings) {
            LINYAPS_BOX_LOG_WARN(
              "mount {}: inline idmap uid mapping ignored because uidMappings is specified",
              v.destination);
        } else {
            v.uid_mappings = std::move(inline_uid_mappings);
        }
    }

    if (inline_gid_mappings) {
        if (v.gid_mappings) {
            LINYAPS_BOX_LOG_WARN(
              "mount {}: inline idmap gid mapping ignored because gidMappings is specified",
              v.destination);
        } else {
            v.gid_mappings = std::move(inline_gid_mappings);
        }
    }

    if (UNLIKELY(!have_destination)) {
        throw std::runtime_error("mount.destination is required");
    }
}

void validate(const mount &v)
{
    if (UNLIKELY(!v.destination.is_absolute())) {
        throw std::runtime_error(
          fmt::format("mount destination must be an absolute path: {}", v.destination));
    }

    if (UNLIKELY(v.uid_mappings.has_value() != v.gid_mappings.has_value())) {
        throw std::runtime_error(
          "uidMappings and gidMappings on mounts must be specified together");
    }
}

} // namespace linyaps_box::config
