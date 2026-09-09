// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linyaps_box/os/mm.h"

namespace linyaps_box::os {

auto mmap(void *ptr,
          std::size_t length,
          utils::bitflags<sys::prot_flag> prots,
          utils::bitflags<sys::map_flag> maps,
          utils::file_descriptor_ref fd,
          off_t offset) noexcept -> Result<void *>
{
    auto *ret = ::mmap(ptr, length, prots.to_raw(), static_cast<int>(maps.to_raw()), fd, offset);
    if (UNLIKELY(ret == MAP_FAILED)) {
        return unexpected{ make_error_code(errno) };
    }

    return ret;
}

auto mmap_anonymous(void *ptr,
                    std::size_t length,
                    utils::bitflags<sys::prot_flag> prots,
                    utils::bitflags<sys::map_flag> maps) noexcept -> Result<void *>
{
    auto *ret =
      ::mmap(ptr, length, prots.to_raw(), static_cast<int>(maps.to_raw() | MAP_ANONYMOUS), -1, 0);
    if (UNLIKELY(ret == MAP_FAILED)) {
        return unexpected{ make_error_code(errno) };
    }

    return ret;
}

auto munmap(void *ptr, std::size_t length) noexcept -> Result<void>
{
    auto ret = ::munmap(ptr, length);
    if (UNLIKELY(ret == -1)) {
        return unexpected{ make_error_code(errno) };
    }

    return { };
}

} // namespace linyaps_box::os
