// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/config/id_mapping.h"
#include "linyaps_box/utils/enum_traits.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linyaps_box::config {

enum class vfs_flag : uint32_t {
    none = 0,
    bind = 1U << 0,
    rec = 1U << 1,
    ro = 1U << 2,
    nosuid = 1U << 3,
    nodev = 1U << 4,
    noexec = 1U << 5,
    noatime = 1U << 6,
    nodiratime = 1U << 7,
    relatime = 1U << 8,
    strictatime = 1U << 9,
    sync = 1U << 10,
    dirsync = 1U << 11,
    iversion = 1U << 12,
    lazytime = 1U << 13,
    mand = 1U << 14,
    silent = 1U << 15,
    nosymfollow = 1U << 16,
    remount = 1U << 17,
};

LINYAPS_ENABLE_BITMASK_ENUM(vfs_flag);

LINYAPS_REGISTER_ENUM_TABLE(vfs_flag,
                            19,
                            { vfs_flag::none, "defaults" },
                            { vfs_flag::bind, "bind" },
                            { vfs_flag::rec, "rec" },
                            { vfs_flag::ro, "ro" },
                            { vfs_flag::nosuid, "nosuid" },
                            { vfs_flag::nodev, "nodev" },
                            { vfs_flag::noexec, "noexec" },
                            { vfs_flag::noatime, "noatime" },
                            { vfs_flag::nodiratime, "nodiratime" },
                            { vfs_flag::relatime, "relatime" },
                            { vfs_flag::strictatime, "strictatime" },
                            { vfs_flag::sync, "sync" },
                            { vfs_flag::dirsync, "dirsync" },
                            { vfs_flag::iversion, "iversion" },
                            { vfs_flag::lazytime, "lazytime" },
                            { vfs_flag::mand, "mand" },
                            { vfs_flag::silent, "silent" },
                            { vfs_flag::nosymfollow, "nosymfollow" },
                            { vfs_flag::remount, "remount" })

enum class propagation_flag : uint8_t {
    none = 0,
    private_ = 1U << 0,
    slave = 1U << 1,
    shared = 1U << 2,
    unbindable = 1U << 3,
    rec = 1U << 4,
};

LINYAPS_ENABLE_BITMASK_ENUM(propagation_flag);

LINYAPS_REGISTER_ENUM_TABLE(propagation_flag,
                            6,
                            { propagation_flag::none, "none" },
                            { propagation_flag::private_, "private" },
                            { propagation_flag::slave, "slave" },
                            { propagation_flag::shared, "shared" },
                            { propagation_flag::unbindable, "unbindable" },
                            { propagation_flag::rec, "rec" })

enum class recursive_attr_flag : std::uint8_t {
    none = 0,
    rdonly = 1U << 0,
    nosuid = 1U << 1,
    nodev = 1U << 2,
    noexec = 1U << 3,
    noatime = 1U << 4,
    nodiratime = 1U << 5,
    strictatime = 1U << 6,
    nosymfollow = 1U << 7,
};

LINYAPS_ENABLE_BITMASK_ENUM(recursive_attr_flag);

LINYAPS_REGISTER_ENUM_TABLE(recursive_attr_flag,
                            9,
                            { recursive_attr_flag::none, "none" },
                            { recursive_attr_flag::rdonly, "rdonly" },
                            { recursive_attr_flag::nosuid, "nosuid" },
                            { recursive_attr_flag::nodev, "nodev" },
                            { recursive_attr_flag::noexec, "noexec" },
                            { recursive_attr_flag::noatime, "noatime" },
                            { recursive_attr_flag::nodiratime, "nodiratime" },
                            { recursive_attr_flag::strictatime, "strictatime" },
                            { recursive_attr_flag::nosymfollow, "nosymfollow" })

struct mount
{
    enum class idmap_type : std::uint8_t { idmap, ridmap };

    enum class extension : std::uint8_t {
        none = 0,
        copy_symlink = (1U << 0),
        tmpcopyup = (1U << 1),
    };

    struct recursive_attr
    {
        utils::bitflags<recursive_attr_flag> set;
        utils::bitflags<recursive_attr_flag> clr;
    };

    utils::bitflags<vfs_flag> vfs_flags;
    utils::bitflags<propagation_flag> propagation_flags;
    std::optional<recursive_attr> rec_attr;
    extension extension_flags{ extension::none };
    std::optional<idmap_type> idmap;

    std::optional<std::string> source;
    std::filesystem::path destination;
    std::optional<std::string> type;
    std::string data;

    std::optional<std::vector<id_mapping>> uid_mappings;
    std::optional<std::vector<id_mapping>> gid_mappings;
};

LINYAPS_ENABLE_BITMASK_ENUM(mount::extension);

LINYAPS_REGISTER_ENUM_TABLE(mount::extension,
                            3,
                            { mount::extension::none, "none" },
                            { mount::extension::copy_symlink, "copy-symlink" },
                            { mount::extension::tmpcopyup, "tmpcopyup" })

LINYAPS_REGISTER_ENUM_TABLE(mount::idmap_type,
                            2,
                            { mount::idmap_type::idmap, "idmap" },
                            { mount::idmap_type::ridmap, "ridmap" })

void from_json(const nlohmann::json &j, mount &v);

void validate(const mount &v);

} // namespace linyaps_box::config
