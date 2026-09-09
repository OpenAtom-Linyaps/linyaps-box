// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linyaps_box/os/result.h"
#include "linyaps_box/utils/enum_traits.h"
#include "linyaps_box/utils/file_describer.h"

#include <sys/mman.h>

namespace linyaps_box::os {

namespace sys {

enum class prot_flag : std::uint8_t {
    none = PROT_NONE,
    read = PROT_READ,
    write = PROT_WRITE,
    exec = PROT_EXEC,
};
LINYAPS_ENABLE_BITMASK_ENUM(prot_flag);
LINYAPS_REGISTER_ENUM_TABLE(prot_flag,
                            4,
                            { prot_flag::none, "PROT_NONE" },
                            { prot_flag::read, "PROT_READ" },
                            { prot_flag::write, "PROT_WRITE" },
                            { prot_flag::exec, "PROT_EXEC" });

enum class map_flag : std::uint32_t {
    shared = MAP_SHARED,
    shared_validate = MAP_SHARED_VALIDATE,
    private_ = MAP_PRIVATE,
    fixed = MAP_FIXED,
    fixed_noreplace = MAP_FIXED_NOREPLACE,
    growsdown = MAP_GROWSDOWN,
    hugetlb = MAP_HUGETLB,
    locked = MAP_LOCKED,
    noreserve = MAP_NORESERVE,
    populate = MAP_POPULATE,
    stack = MAP_STACK,
#ifdef MAP_SYNC
    sync = MAP_SYNC // unavailble on mips
#endif
};

#ifdef MAP_SYNC
#  define LINYAPS_BOX_MAP_FLAG_SYNC_ENTRY { map_flag::sync, "MAP_SYNC" },
constexpr auto map_flag_table_count = 12;
#else
#  define LINYAPS_BOX_MAP_FLAG_SYNC_ENTRY
constexpr auto map_flag_table_count = 11;
#endif

LINYAPS_ENABLE_BITMASK_ENUM(map_flag);
LINYAPS_REGISTER_ENUM_TABLE(map_flag,
                            map_flag_table_count,
                            { map_flag::shared, "MAP_SHARED" },
                            { map_flag::shared_validate, "MAP_SHARED_VALIDATE" },
                            { map_flag::private_, "MAP_PRIVATE" },
                            { map_flag::fixed, "MAP_FIXED" },
                            { map_flag::fixed_noreplace, "MAP_FIXED_NOREPLACE" },
                            { map_flag::growsdown, "MAP_GROWSDOWN" },
                            { map_flag::hugetlb, "MAP_HUGETLB" },
                            { map_flag::locked, "MAP_LOCKED" },
                            { map_flag::noreserve, "MAP_NORESERVE" },
                            { map_flag::populate, "MAP_POPULATE" },
                            { map_flag::stack, "MAP_STACK" },
                            LINYAPS_BOX_MAP_FLAG_SYNC_ENTRY)
#undef LINYAPS_BOX_MAP_FLAG_SYNC_ENTRY

} // namespace sys

[[nodiscard]] auto mmap(void *ptr,
                        std::size_t length,
                        utils::bitflags<sys::prot_flag> prots,
                        utils::bitflags<sys::map_flag> maps,
                        utils::file_descriptor_ref fd,
                        off_t offset) noexcept -> Result<void *>;

[[nodiscard]] auto mmap_anonymous(void *ptr,
                                  std::size_t length,
                                  utils::bitflags<sys::prot_flag> prots,
                                  utils::bitflags<sys::map_flag> maps) noexcept -> Result<void *>;

[[nodiscard]] auto munmap(void *ptr, std::size_t length) noexcept -> Result<void>;
} // namespace linyaps_box::os
