// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/mount.h"

#include "linyaps_box/config/utils.h"
#include "linyaps_box/log/macro.h"
#include "linyaps_box/os/kernel_constants.h"

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

constexpr auto extra_flags_table = get_enum_table_from<mount::extension>();
constexpr auto idmap_options_table = get_enum_table_from<mount::idmap_type>();

struct vfs_option_entry
{
    std::string_view name;
    utils::bitflags<vfs_flag> flag;
    bool clear;
};

constexpr std::array<vfs_option_entry, 33> vfs_options{ {
  { "bind", vfs_flag::bind, false },
  { "dirsync", vfs_flag::dirsync, false },
  { "defaults", vfs_flag::defaults, false },
  { "iversion", vfs_flag::iversion, false },
  { "lazytime", vfs_flag::lazytime, false },
  { "mand", vfs_flag::mand, false },
  { "noatime", vfs_flag::noatime, false },
  { "nodev", vfs_flag::nodev, false },
  { "nodiratime", vfs_flag::nodiratime, false },
  { "noexec", vfs_flag::noexec, false },
  { "nosuid", vfs_flag::nosuid, false },
  { "nosymfollow", vfs_flag::nosymfollow, false },
  { "rbind", vfs_flag::bind | vfs_flag::rec, false },
  { "relatime", vfs_flag::relatime, false },
  { "remount", vfs_flag::remount, false },
  { "ro", vfs_flag::ro, false },
  { "silent", vfs_flag::silent, false },
  { "strictatime", vfs_flag::strictatime, false },
  { "sync", vfs_flag::sync, false },
  { "async", vfs_flag::sync, true },
  { "atime", vfs_flag::noatime, true },
  { "dev", vfs_flag::nodev, true },
  { "diratime", vfs_flag::nodiratime, true },
  { "exec", vfs_flag::noexec, true },
  { "loud", vfs_flag::silent, true },
  { "noiversion", vfs_flag::iversion, true },
  { "nolazytime", vfs_flag::lazytime, true },
  { "nomand", vfs_flag::mand, true },
  { "norelatime", vfs_flag::relatime, true },
  { "nostrictatime", vfs_flag::strictatime, true },
  { "rw", vfs_flag::ro, true },
  { "suid", vfs_flag::nosuid, true },
  { "symfollow", vfs_flag::nosymfollow, true },
} };

struct propagation_option_entry
{
    std::string_view name;
    utils::bitflags<propagation_flag> flag;
};

constexpr std::array<propagation_option_entry, 8> propagation_options{ {
  { "private", propagation_flag::private_ },
  { "rprivate", propagation_flag::private_ | propagation_flag::rec },
  { "slave", propagation_flag::slave },
  { "rslave", propagation_flag::slave | propagation_flag::rec },
  { "shared", propagation_flag::shared },
  { "rshared", propagation_flag::shared | propagation_flag::rec },
  { "unbindable", propagation_flag::unbindable },
  { "runbindable", propagation_flag::unbindable | propagation_flag::rec },
} };

// Recursive mount_setattr options. these carry raw
// MOUNT_ATTR_* UAPI values (mount::recursive_attr is a passthrough ABI bitmask;
// mount_setattr(2) is not implemented yet).
// TODO: Revisit when it lands.
struct rec_attr_option_entry
{
    std::string_view name;
    uint64_t value;
};

constexpr std::array<rec_attr_option_entry, 9> recursive_attr_set{ {
  { "rro", os::sys::mount_attr_rdonly },
  { "rnosuid", os::sys::mount_attr_nosuid },
  { "rnodev", os::sys::mount_attr_nodev },
  { "rnoexec", os::sys::mount_attr_noexec },
  { "rnodiratime", os::sys::mount_attr_nodiratime },
  { "rnoatime", os::sys::mount_attr_noatime },
  { "rstrictatime", os::sys::mount_attr_strictatime },
  { "rnosymfollow", os::sys::mount_attr_nosymfollow },
  { "rrelatime", 0 },
} };

constexpr std::array<rec_attr_option_entry, 9> recursive_attr_clr{ {
  { "rrw", os::sys::mount_attr_rdonly },
  { "rsuid", os::sys::mount_attr_nosuid },
  { "rdev", os::sys::mount_attr_nodev },
  { "rexec", os::sys::mount_attr_noexec },
  { "rdiratime", os::sys::mount_attr_nodiratime },
  { "ratime", os::sys::mount_attr_noatime },
  { "rnostrictatime", os::sys::mount_attr_strictatime },
  { "rsymfollow", os::sys::mount_attr_nosymfollow },
  { "rnorelatime", 0 },
} };

template <typename Entry, std::size_t N>
constexpr const Entry *find_option(const std::array<Entry, N> &table, std::string_view key) noexcept
{
    for (const auto &entry : table) {
        if (entry.name == key) {
            return &entry;
        }
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
    mount::idmap_type type;
    std::optional<std::vector<id_mapping>> uid_mappings;
    std::optional<std::vector<id_mapping>> gid_mappings;
};

auto parse_inline_idmap_option(std::string_view opt) -> inline_idmap_result
{
    auto eq_pos = opt.find('=');
    auto prefix = opt.substr(0, eq_pos);
    auto iv = idmap_options_table.from_name(prefix);
    if (UNLIKELY(!iv)) {
        throw std::runtime_error(fmt::format("unknown idmap option: {}", opt));
    }

    inline_idmap_result result;
    result.type = *iv;
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

auto parse_mount_options(const std::vector<std::string> &options) -> parsed_mount_options
{
    parsed_mount_options result;

    auto ensure_idmap_compatible = [&result](mount::idmap_type candidate) {
        if (UNLIKELY(result.idmap.has_value() && *result.idmap != candidate)) {
            throw std::runtime_error("idmap and ridmap options are mutually exclusive");
        }
    };

    for (const auto &opt : options) {
        if (const auto *entry = find_option(vfs_options, opt)) {
            if (entry->clear) {
                result.vfs_flags &= ~entry->flag;
            } else {
                result.vfs_flags |= entry->flag;
            }

            continue;
        }

        if (const auto *entry = find_option(propagation_options, opt)) {
            result.propagation_flags |= entry->flag;
            continue;
        }

        if (const auto *entry = find_option(recursive_attr_set, opt)) {
            if (!result.rec_attr) {
                result.rec_attr.emplace();
            }

            result.rec_attr->set |= entry->value;
            continue;
        }

        if (const auto *entry = find_option(recursive_attr_clr, opt)) {
            if (!result.rec_attr) {
                result.rec_attr.emplace();
            }

            result.rec_attr->clr |= entry->value;
            continue;
        }

        if (auto ev = extra_flags_table.from_name(opt)) {
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
        if (auto eq_pos = opt.find('='); UNLIKELY(eq_pos != std::string_view::npos)) {
            auto prefix = std::string_view{ opt }.substr(0, eq_pos);
            if (auto iv = idmap_options_table.from_name(prefix)) {
                ensure_idmap_compatible(*iv);

                auto inline_result = parse_inline_idmap_option(opt);
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
                auto options = val.get<std::vector<std::string>>();
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
        LINYAPS_BOX_LOG_WARN("mount destination is not an absolute path: {}", v.destination);
    }

    if (UNLIKELY(v.uid_mappings.has_value() != v.gid_mappings.has_value())) {
        throw std::runtime_error(
          "uidMappings and gidMappings on mounts must be specified together");
    }
}

} // namespace linyaps_box::config
