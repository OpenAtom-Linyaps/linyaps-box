// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/mount.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"

#include <fmt/std.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace linyaps_box::config {

namespace {

enum class option_kind : std::uint8_t { vfs, propagation, rec_attr_set, rec_attr_clr };

struct mount_option
{
    std::string_view name;
    uint32_t value;
    option_kind kind;
    bool clear;
};

// from https://github.com/opencontainers/runtime-spec/blob/main/config.md#linux-mount-options
constexpr std::array<mount_option, 59> mount_options{
    {
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
      {
        "rbind",
        static_cast<uint32_t>((vfs_flag::bind | vfs_flag::rec).to_raw()),
        option_kind::vfs,
        false,
      },
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
      {
        "private",
        static_cast<uint32_t>(propagation_flag::private_),
        option_kind::propagation,
        false,
      },
      {
        "rprivate",
        static_cast<uint32_t>((propagation_flag::private_ | propagation_flag::rec).to_raw()),
        option_kind::propagation,
        false,
      },
      { "slave", static_cast<uint32_t>(propagation_flag::slave), option_kind::propagation, false },
      {
        "rslave",
        static_cast<uint32_t>((propagation_flag::slave | propagation_flag::rec).to_raw()),
        option_kind::propagation,
        false,
      },
      {
        "shared",
        static_cast<uint32_t>(propagation_flag::shared),
        option_kind::propagation,
        false,
      },
      {
        "rshared",
        static_cast<uint32_t>((propagation_flag::shared | propagation_flag::rec).to_raw()),
        option_kind::propagation,
        false,
      },
      {
        "unbindable",
        static_cast<uint32_t>(propagation_flag::unbindable),
        option_kind::propagation,
        false,
      },
      {
        "runbindable",
        static_cast<uint32_t>((propagation_flag::unbindable | propagation_flag::rec).to_raw()),
        option_kind::propagation,
        false,
      },
      // recursive mount_setattr
      {
        "rro",
        static_cast<uint32_t>(recursive_attr_flag::rdonly),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnosuid",
        static_cast<uint32_t>(recursive_attr_flag::nosuid),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnodev",
        static_cast<uint32_t>(recursive_attr_flag::nodev),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnoexec",
        static_cast<uint32_t>(recursive_attr_flag::noexec),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnodiratime",
        static_cast<uint32_t>(recursive_attr_flag::nodiratime),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnoatime",
        static_cast<uint32_t>(recursive_attr_flag::noatime),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rstrictatime",
        static_cast<uint32_t>(recursive_attr_flag::strictatime),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rnosymfollow",
        static_cast<uint32_t>(recursive_attr_flag::nosymfollow),
        option_kind::rec_attr_set,
        false,
      },
      {
        "rrelatime",
        0U,
        option_kind::rec_attr_set,
        false,
      },
      {
        "rrw",
        static_cast<uint32_t>(recursive_attr_flag::rdonly),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rsuid",
        static_cast<uint32_t>(recursive_attr_flag::nosuid),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rdev",
        static_cast<uint32_t>(recursive_attr_flag::nodev),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rexec",
        static_cast<uint32_t>(recursive_attr_flag::noexec),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rdiratime",
        static_cast<uint32_t>(recursive_attr_flag::nodiratime),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "ratime",
        static_cast<uint32_t>(recursive_attr_flag::noatime),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rnostrictatime",
        static_cast<uint32_t>(recursive_attr_flag::strictatime),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rsymfollow",
        static_cast<uint32_t>(recursive_attr_flag::nosymfollow),
        option_kind::rec_attr_clr,
        false,
      },
      {
        "rnorelatime",
        0U,
        option_kind::rec_attr_clr,
        false,
      },
    },
};

constexpr auto option_name_less(std::string_view left, std::string_view right) noexcept -> bool
{
    return left.size() == right.size() ? left < right : left.size() < right.size();
}

constexpr auto make_sorted_mount_options() noexcept
  -> std::array<mount_option, mount_options.size()>
{
    auto out = mount_options;
    linyaps_box::utils::detail::shell_sort(
      out,
      [](const mount_option &a, const mount_option &b) noexcept {
          return option_name_less(a.name, b.name);
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
                                                return option_name_less(entry.name, n);
                                            });
    if (it != sorted_mount_options.cend() && it->name == name) {
        return &*it;
    }

    return nullptr;
}

// Recognizes the spec's bare `idmap`/`ridmap` mount options (idmapped mounts).
// The inline `idmap=uids=…,gids=…` form is not part of the spec and is not
// interpreted: such tokens fall through to the raw data string.
void set_idmap(mount &target, mount::idmap_type type)
{
    if (UNLIKELY(target.idmap.has_value() && *target.idmap != type)) {
        throw json::decode_error{ "idmap and ridmap options are mutually exclusive" };
    }

    target.idmap.emplace(type);
}

void apply_known_option(mount &target, const mount_option &entry)
{
    switch (entry.kind) {
    case option_kind::vfs: {
        const auto flag =
          utils::bitflags<vfs_flag>::from_raw_truncate(static_cast<std::uint32_t>(entry.value));
        if (entry.clear) {
            target.vfs_flags &= ~flag;
        } else {
            target.vfs_flags |= flag;
        }
    } break;
    case option_kind::propagation: {
        target.propagation_flags |= utils::bitflags<propagation_flag>::from_raw_truncate(
          static_cast<std::uint8_t>(entry.value));
    } break;
    case option_kind::rec_attr_set: {
        if (!target.rec_attr) {
            target.rec_attr.emplace();
        }

        target.rec_attr->set |= utils::bitflags<recursive_attr_flag>::from_raw_truncate(
          static_cast<std::uint8_t>(entry.value));
    } break;
    case option_kind::rec_attr_clr: {
        if (!target.rec_attr) {
            target.rec_attr.emplace();
        }

        target.rec_attr->clr |= utils::bitflags<recursive_attr_flag>::from_raw_truncate(
          static_cast<std::uint8_t>(entry.value));
    } break;
    }
}

void append_mount_data(mount &target, std::string_view opt)
{
    if (!target.data.empty()) {
        target.data.push_back(',');
    }

    target.data.append(opt);
}

void decode_mount_option(std::string_view option, mount &v)
{
    if (const auto *entry = find_mount_option(option)) {
        apply_known_option(v, *entry);
        return;
    }

    if (const auto extension = utils::enum_table_v<mount::extension>.from_name(option)) {
        v.extension_flags |= *extension;
        return;
    }

    // Only the exact `idmap`/`ridmap` tokens are the spec option; a value (e.g.
    // `idmap=uids=0:1000:1`) is the removed inline form and falls to data.
    if (const auto type = utils::enum_table_v<mount::idmap_type>.from_name(option)) {
        set_idmap(v, *type);
        return;
    }

    append_mount_data(v, option);
}

struct mount_schema
{
    static constexpr auto fields()
    {
        using json::consume_strings;
        using json::field;
        return std::make_tuple(field<&mount::destination>("destination").required(),
                               field<&mount::source>("source").ignore_null(),
                               field<&mount::type>("type").ignore_null(),
                               field<&mount::uid_mappings>("uidMappings").ignore_null(),
                               field<&mount::gid_mappings>("gidMappings").ignore_null(),
                               consume_strings<&decode_mount_option>("options").ignore_null());
    }
};

} // namespace

auto ops_of(json::object_tag<mount>) -> const json::frame_ops &
{
    return json::make_object_ops<mount, mount_schema>();
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
