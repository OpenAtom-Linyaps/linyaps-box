// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/config/mount.h"

#include "linyaps_box/config/validation.h"
#include "linyaps_box/json/parser.h"
#include "linyaps_box/log/macro.h"

#include <fmt/std.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
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

constexpr auto make_sorted_mount_options() noexcept
  -> std::array<mount_option, mount_options.size()>
{
    auto out = mount_options;
    linyaps_box::utils::detail::shell_sort(
      out,
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

[[nodiscard]] auto apply_extension(mount &target, std::string_view opt) -> bool
{
    const auto extension = utils::enum_table_v<mount::extension>.from_name(opt);
    if (!extension) {
        return false;
    }

    target.extension_flags |= *extension;
    return true;
}

[[nodiscard]] auto apply_idmap_flag(mount &target, std::string_view opt) -> bool
{
    const auto type = utils::enum_table_v<mount::idmap_type>.from_name(opt);
    if (!type) {
        return false;
    }

    set_idmap(target, *type);
    return true;
}

void append_mount_data(mount &target, std::string_view opt)
{
    if (!target.data.empty()) {
        target.data.push_back(',');
    }

    target.data.append(opt);
}

auto parse_id_mapping_chunk(std::string_view chunk) -> id_mapping
{
    // format: "containerID:hostID:size"
    auto first_colon = chunk.find(':');
    if (UNLIKELY(first_colon == std::string_view::npos)) {
        throw json::decode_error(fmt::format("invalid id mapping: {}", chunk));
    }

    auto second_colon = chunk.find(':', first_colon + 1);
    if (UNLIKELY(second_colon == std::string_view::npos)) {
        throw json::decode_error(fmt::format("invalid id mapping: {}", chunk));
    }

    id_mapping mapping{ };
    const char *const base = chunk.data();
    auto [p1, ec1] = std::from_chars(base, base + first_colon, mapping.container_id);
    // from_chars may stop early on trailing garbage; the whole field must be consumed.
    if (UNLIKELY(ec1 != std::errc{ } || p1 != base + first_colon)) {
        throw json::decode_error(fmt::format("invalid container id in mapping: {}", chunk));
    }

    auto [p2, ec2] = std::from_chars(base + first_colon + 1, base + second_colon, mapping.host_id);
    if (UNLIKELY(ec2 != std::errc{ } || p2 != base + second_colon)) {
        throw json::decode_error(fmt::format("invalid host id in mapping: {}", chunk));
    }

    auto [p3, ec3] = std::from_chars(base + second_colon + 1, base + chunk.size(), mapping.size);
    if (UNLIKELY(ec3 != std::errc{ } || p3 != base + chunk.size())) {
        throw json::decode_error(fmt::format("invalid size in mapping: {}", chunk));
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
            throw json::decode_error(fmt::format("invalid id mapping option: {}", opt));
        }

        auto key = part.substr(0, eq2_pos);
        auto value = part.substr(eq2_pos + 1);

        if (key == "uids") {
            result.uid_mappings = parse_mappings(value);
        } else if (key == "gids") {
            result.gid_mappings = parse_mappings(value);
        } else {
            throw json::decode_error(fmt::format("unknown id mapping key: {}", key));
        }

        if (comma_pos == std::string_view::npos) {
            break;
        }

        rest = rest.substr(comma_pos + 1);
    }

    return result;
}

// The schema uses bits [0, 6); the remaining bits track mount-local parsing
// state. They never become part of the returned mount model.
enum mapping_state : std::size_t {
    explicit_uid = 6,
    explicit_gid,
    inline_uid,
    inline_gid,
};

void parse_option(std::string_view option, mount &v, std::bitset<json::detail::max_fields> &state)
{
    if (const auto *entry = find_mount_option(option)) {
        apply_known_option(v, *entry);
        return;
    }

    if (apply_extension(v, option) || apply_idmap_flag(v, option)) {
        return;
    }

    if (const auto equal = option.find('='); equal != std::string_view::npos) {
        if (const auto type =
              utils::enum_table_v<mount::idmap_type>.from_name(option.substr(0, equal))) {
            set_idmap(v, *type);
            auto mappings = parse_inline_idmap_option(option, *type);
            state.set(inline_uid, mappings.uid_mappings.has_value());
            state.set(inline_gid, mappings.gid_mappings.has_value());
            if (!state.test(explicit_uid)) {
                v.uid_mappings = std::move(mappings.uid_mappings);
            }
            if (!state.test(explicit_gid)) {
                v.gid_mappings = std::move(mappings.gid_mappings);
            }
            return;
        }
    }

    append_mount_data(v, option);
}

struct mount_options_ops : json::detail::frame_handler<mount_options_ops>
{
    static auto segment(const json::detail::frame &f) -> json::detail::location_edge
    {
        return json::detail::location_edge::element(f.active);
    }

    static auto on_key(json::detail::frame &f,
                       json::detail::frame_stack &st,
                       [[maybe_unused]] std::string &key) -> bool
    {
        json::detail::raise_decoded(st, segment(f), "unexpected object key inside an array");
    }

    static void on_value(json::detail::frame &f,
                         json::detail::frame_stack &st,
                         const json::detail::value_view &value)
    {
        const auto index = f.active++;
        try {
            parse_option(value.as_string_view(), f.target.as<mount>(), f.seen);
        } catch (const json::decode_error &e) {
            json::detail::raise_decoded(st, json::detail::location_edge::element(index), e.what());
        }
    }

    static void on_object_begin(json::detail::frame &f, json::detail::frame_stack &st)
    {
        json::detail::raise_decoded(st, segment(f), "type must be string, but is object");
    }

    static void on_array_begin(json::detail::frame &f, json::detail::frame_stack &st)
    {
        json::detail::raise_decoded(st, segment(f), "type must be string, but is array");
    }

    static void on_end(json::detail::frame &f, json::detail::frame_stack &st)
    {
        const auto has_inline_uid = f.seen.test(inline_uid);
        const auto has_inline_gid = f.seen.test(inline_gid);
        st.pop();
        st.top().seen.set(inline_uid, has_inline_uid);
        st.top().seen.set(inline_gid, has_inline_gid);
    }
};

struct mount_options_tag
{
    using target_type = mount;
};

auto ops_of(mount_options_tag) -> const json::frame_ops &
{
    static constexpr auto ops = mount_options_ops::make();
    return ops;
}

struct mount_options_field : json::detail::field_descriptor<mount_options_field>
{
    using field_descriptor::field_descriptor;
    static constexpr auto kind = json::detail::field_kind::array;

    void assign_value(const json::detail::value_view &value, [[maybe_unused]] mount &target) const
    {
        if (!skip_null(value)) {
            value.raise_expected("array");
        }
    }

    void begin_array(json::detail::frame_stack &st, mount &target) const
    {
        const auto has_explicit_uid = st.top().seen.test(explicit_uid);
        const auto has_explicit_gid = st.top().seen.test(explicit_gid);
        st.push_frame(target, mount_options_tag{ });
        auto &f = st.top();
        f.active = 0;
        f.seen.set(explicit_uid, has_explicit_uid);
        f.seen.set(explicit_gid, has_explicit_gid);
    }
};

struct mount_schema
{
    static constexpr auto fields()
    {
        using json::field;
        return std::make_tuple(field<&mount::destination>("destination").required(),
                               field<&mount::source>("source").ignore_null(),
                               field<&mount::type>("type").ignore_null(),
                               field<&mount::uid_mappings>("uidMappings").ignore_null(),
                               field<&mount::gid_mappings>("gidMappings").ignore_null(),
                               mount_options_field{ "options" }.ignore_null());
    }
};

struct mount_ops : json::detail::object_frame_ops<mount, mount_schema>
{
    using base = json::detail::object_frame_ops<mount, mount_schema>;
    static constexpr std::size_t uid_field = 3;
    static constexpr std::size_t gid_field = 4;
    static_assert(base::count == explicit_uid && inline_gid < json::detail::max_fields,
                  "mount parsing state must not overlap schema field bits");
    static_assert(std::get<uid_field>(base::table).name() == "uidMappings"
                    && std::get<gid_field>(base::table).name() == "gidMappings",
                  "mount mapping field indices must match the schema");
    static_assert(json::detail::names_are_unique(mount_schema::fields()),
                  "duplicate JSON field key in mount");

    static void on_array_begin(json::detail::frame &f, json::detail::frame_stack &st)
    {
        if (f.active == uid_field) {
            f.seen.set(explicit_uid);
        } else if (f.active == gid_field) {
            f.seen.set(explicit_gid);
        }
        base::on_array_begin(f, st);
    }

    static void on_end(json::detail::frame &f, json::detail::frame_stack &st)
    {
        const auto &target = f.target.as<mount>();
        if (f.seen.test(explicit_uid) && f.seen.test(inline_uid)) {
            LINYAPS_BOX_LOG_WARN(
              "mount {}: inline idmap uid mapping ignored because uidMappings is specified",
              target.destination);
        }
        if (f.seen.test(explicit_gid) && f.seen.test(inline_gid)) {
            LINYAPS_BOX_LOG_WARN(
              "mount {}: inline idmap gid mapping ignored because gidMappings is specified",
              target.destination);
        }
        base::on_end(f, st);
    }
};

} // namespace

auto ops_of(json::object_tag<mount>) -> const json::frame_ops &
{
    static constexpr auto ops = json::detail::frame_handler<mount_ops>::make();
    return ops;
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
