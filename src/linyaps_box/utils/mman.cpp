// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "mman.h"

#include <sys/mman.h>

namespace linyaps_box::utils {
auto memfd_create(const char *name, unsigned int flags) -> file_descriptor
{
    auto fd = ::memfd_create(name, flags);
    if (fd == -1) {
        throw std::system_error(errno, std::system_category(), "memfd_create failed");
    }

    return file_descriptor{ fd };
}

auto mmap(void *addr,
          std::size_t length,
          int prot,
          int flags,
          std::optional<std::reference_wrapper<const file_descriptor>> fd,
          off_t offset) -> std::byte *
{
    auto *result = static_cast<std::byte *>(::mmap(addr,
                                                   length,
                                                   prot,
                                                   flags,
                                                   fd.has_value() ? fd.value().get().get() : -1,
                                                   offset));
    if (result == MAP_FAILED) {
        throw std::system_error(errno, std::system_category(), "mmap failed");
    }

    return result;
}

auto munmap(std::byte *addr, std::size_t length) -> void
{
    auto ret = ::munmap(addr, length);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category(), "munmap failed");
    }
}

} // namespace linyaps_box::utils
