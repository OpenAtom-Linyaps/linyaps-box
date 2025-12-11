// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>

namespace linyaps_box::utils {

template<typename T>
class span
{
public:
    using element_type = T;
    using pointer = T *;
    using size_type = std::size_t;

    constexpr span() noexcept
        : data_(nullptr)
        , size_(0)
    {
    }

    constexpr span(pointer ptr, size_type count) noexcept
        : data_(ptr)
        , size_(count)
    {
    }

    [[nodiscard]] constexpr pointer data() const noexcept { return data_; }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr span subspan(size_type offset, size_type count = -1) const noexcept
    {
        if (offset >= size_) {
            return {};
        }

        size_type actual_count = (count == static_cast<size_type>(-1)) ? (size_ - offset) : count;
        return { data_ + offset, std::min(actual_count, size_ - offset) };
    }

private:
    pointer data_;
    size_type size_;
};

} // namespace linyaps_box::utils
